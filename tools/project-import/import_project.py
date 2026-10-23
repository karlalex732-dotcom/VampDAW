"""Read-only FLP / ALS migration into editable VampDAW projects. GPL-3.0.

FLP event layout interoperability references: demberto/PyFLP (GPL-3.0-or-later),
pyflp/{pattern,arrangement,channel,project}.py, https://github.com/demberto/PyFLP.
FL25 event-size exception and 80-byte playlist: dawhubapp/flpdiff,
docs/fl-format/{fl25-event-format,flp-format-spec}.md on github.com/dawhubapp/flpdiff.
No vendor application, device code, or plugin preset is executed by this reader.
"""
from pathlib import Path
import argparse, gzip, hashlib, json, math, os, struct, subprocess, time, xml.etree.ElementTree as ET

MAX_BYTES=512*1024*1024
MIN_TEMPO=10.
MAX_TEMPO=999.
def number(value, default=0.):
    try:
        n=float(value)
        return n if math.isfinite(n) else default
    except (TypeError,ValueError): return default
def value(node, path, default=''):
    e=node.find(path) if node is not None else None
    return e.get('Value',default) if e is not None else default
def boolean(node,path,default=False): return value(node,path,str(default)).lower()=='true'
def track(name): return dict(name=name,volume=1.,pan=0.,notes=[],patterns=[],clips=[],patternLength=4.,patternName='Pattern 1')
def clip(at,length,pattern=0,offset=0.,name='',audio='',muted=False): return [at,length,pattern,offset,1.,0.,0.,0.,name,-1,muted,audio]

class Importer:
    def __init__(self,source,output,encoder):
        self.source=Path(source).resolve();self.output=Path(output).resolve();self.encoder=Path(encoder)
        self.warnings=[];self.assets={};self.devices=[];self.missing=[];self.initialized_patterns=set()
        self.project=dict(format='VampDAW',version=7,name=self.source.stem+' / Imported',bpm=120.,bars=8,loop=False,tracks=[],master={'name':'Master','volume':.8},folders=[],markers=[])
    def warn(self,text):
        if text not in self.warnings and len(self.warnings)<2000:self.warnings.append(text)
    def set_tempo(self,bpm):
        if not math.isfinite(bpm) or not MIN_TEMPO<=bpm<=MAX_TEMPO:
            raise ValueError(f'Project tempo {bpm:g} BPM is outside the supported range (10–999 BPM). The source project has not been changed.')
        self.project['bpm']=bpm
    def status(self,text,progress):
        path=self.output/'status.json';tmp=path.with_suffix('.tmp');tmp.write_text(json.dumps(dict(stage=text,progress=progress)),encoding='utf-8')
        # Windows readers/antivirus can briefly deny deletion of the old snapshot.
        # Progress is advisory: a locked indicator must not abort a valid import.
        for attempt in range(8):
            try:
                os.replace(tmp,path)
                return True
            except PermissionError:
                time.sleep(.02*(attempt+1))
        return False
    def resolve(self,*names):
        for name in names:
            if not name:continue
            text=os.path.expandvars(str(name)).replace('\\',os.sep).replace('/',os.sep)
            p=Path(text)
            for candidate in [p,self.source.parent/p,self.source.parent/p.name,self.source.parent/'Samples'/'Imported'/p.name,self.source.parent/'Samples'/'Recorded'/p.name]:
                if candidate.is_file():return candidate.resolve()
        missing=next((str(n) for n in names if n),'Unknown sample')
        if missing not in self.missing:self.missing.append(missing)
        self.warn('Missing audio: '+missing)
        return None
    def audio(self,path):
        if not path:return ''
        key=str(path).casefold()
        if key in self.assets:return self.assets[key]
        self.status('Preparing audio: '+path.name,.45)
        target=self.output/'assets'/(hashlib.sha256(str(path).encode()).hexdigest()[:16]+'.wav');target.parent.mkdir(exist_ok=True)
        try:
            result=subprocess.run([str(self.encoder),'-nostdin','-v','error','-i',str(path),'-map','0:a:0','-ac','2','-c:a','pcm_f32le','-y',str(target)],stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0),timeout=300)
            if result.returncode:raise ValueError(result.stderr.decode('utf-8','replace')[-500:])
        except (OSError,ValueError,subprocess.TimeoutExpired) as e:
            self.warn('Audio could not be decoded; kept muted: '+str(path)+' / '+str(e))
            self.assets[key]=''
            target.unlink(missing_ok=True)
            return ''
        self.assets[key]=str(target);return str(target)
    def add_track(self,t):
        if len(self.project['tracks'])>=512:raise ValueError('Import exceeds 512 tracks; split the source project first.')
        self.project['tracks'].append(t);return t
    def add_pattern(self,t,name,length,notes):
        if not .25<=length<=256:raise ValueError('Pattern length exceeds 256 beats; split long clips before importing.')
        if len(notes)>100000:raise ValueError('Imported pattern exceeds 100,000 notes.')
        if id(t) in self.initialized_patterns and t['patternName']==name and t['patternLength']==length and t['notes']==notes:return 0
        for i,p in enumerate(t['patterns'],1):
            if p['name']==name and p['length']==length and p['notes']==notes:return i
        if id(t) not in self.initialized_patterns:
            self.initialized_patterns.add(id(t));t.update(notes=notes,patternLength=length,patternName=name);return 0
        if len(t['patterns'])>=255:raise ValueError('Import exceeds 256 patterns on one track; split the source track first.')
        t['patterns'].append(dict(name=name,length=length,notes=notes));return len(t['patterns'])
    def instrument_warning(self,t,names):
        if names:
            self.devices.append(dict(track=t['name'],devices=names))
            self.warn('Assign instruments/effects on '+t['name']+': '+', '.join(names))
    def als(self):
        self.status('Reading ALS arrangement',.05)
        with self.source.open('rb') as f:magic=f.read(2)
        opener=gzip.open if magic==b'\x1f\x8b' else open
        with opener(self.source,'rb') as f:data=f.read(MAX_BYTES+1)
        if len(data)>MAX_BYTES:raise ValueError('Expanded ALS exceeds 512 MB.')
        if b'<!DOCTYPE' in data.upper() or b'<!ENTITY' in data.upper():raise ValueError('XML entity declarations are not accepted.')
        root=ET.fromstring(data);live=root.find('LiveSet')
        if root.tag!='Ableton' or live is None:raise ValueError('This is not an Ableton Live Set.')
        master=live.find('MainTrack')
        if master is None:master=live.find('MasterTrack')
        bpm=number(value(master,'DeviceChain/Mixer/Tempo/Manual',120),120)
        self.set_tempo(bpm)
        if master is not None:self.project['master']['volume']=number(value(master,'DeviceChain/Mixer/Volume/Manual',.8),.8)
        folders={};pending_groups=[];all_tracks=live.find('Tracks')
        if all_tracks is None:raise ValueError('ALS has no track list.')
        for node in all_tracks:
            if node.tag not in ('MidiTrack','AudioTrack','GroupTrack','ReturnTrack'):continue
            name=value(node,'Name/UserName') or value(node,'Name/EffectiveName') or node.tag
            if node.tag=='GroupTrack':
                fid=len(folders);folders[node.get('Id')]=fid;self.project['folders'].append(dict(id=fid,parent=-1,name=name,collapsed=False));pending_groups.append((fid,value(node,'TrackGroupId','-1')));self.warn('Group bus processing is not transferred: '+name);continue
            t=self.add_track(track(name));group=value(node,'TrackGroupId','-1')
            if group in folders:t['folder']=folders[group]
            t['volume']=number(value(node,'DeviceChain/Mixer/Volume/Manual',1),1)
            t['pan']=number(value(node,'DeviceChain/Mixer/Pan/Manual',0))
            t['mute']=not boolean(node,'DeviceChain/Mixer/Speaker/Manual',True)
            t['solo']=boolean(node,'DeviceChain/Mixer/SoloSink',False)
            devices=node.find('DeviceChain/DeviceChain/Devices')
            self.instrument_warning(t,[d.tag for d in devices] if devices is not None else [])
            sequencer=node.find('DeviceChain/MainSequencer')
            events=[]
            if sequencer is not None:
                for automation in sequencer.findall('./ClipTimeable/ArrangerAutomation/Events')+sequencer.findall('./Sample/ArrangerAutomation/Events')+sequencer.findall('./Midi/ArrangerAutomation/Events'):
                    events.extend(list(automation))
                slots=sequencer.findall('.//ClipSlotList//MidiClip')+sequencer.findall('.//ClipSlotList//AudioClip')
                if slots:self.warn('Session clips are not placed on the timeline: '+name+' ('+str(len(slots))+').')
            for c in events:
                if c.tag not in ('MidiClip','AudioClip'):continue
                start=number(value(c,'CurrentStart',c.get('Time',0)));end=number(value(c,'CurrentEnd',start+4));length=end-start
                if start<0 or end>16384 or length<=0:raise ValueError('Invalid or out-of-range ALS clip position.')
                cname=value(c,'Name',name);muted=boolean(c,'Disabled',False)
                if c.tag=='MidiClip':
                    loop_start=number(value(c,'Loop/LoopStart',0));loop_end=number(value(c,'Loop/LoopEnd',4));relative=number(value(c,'Loop/StartRelative',0));looped=boolean(c,'Loop/LoopOn',True)
                    if looped and loop_end<=loop_start:raise ValueError("Invalid ALS loop boundaries.")
                    period=max(.25,loop_end-loop_start) if looped else length
                    if period>256:raise ValueError('ALS MIDI clip exceeds 256 beats; split it before importing.')
                    source_notes=[]
                    for kt in c.findall('./Notes/KeyTracks/KeyTrack'):
                        key=int(number(value(kt,'MidiKey',60),60))
                        for n in kt.findall('./Notes/MidiNoteEvent'):
                            pos=number(n.get('Time'));duration=number(n.get('Duration'),.25);vel=number(n.get('Velocity'),100)/127
                            source_notes.append((pos,duration,key,vel,n.get('IsEnabled','true')!='false'))
                    # Trim the first partial loop, then retain editable repeated patterns.
                    elapsed=0.;origin=loop_start+(relative%(loop_end-loop_start) if looped else relative);iterations=0
                    while elapsed<length-1e-8:
                        span=min(length-elapsed,(loop_end-origin) if looped else length)
                        if span<=1e-8:origin=loop_start;continue
                        notes=[]
                        for pos,duration,key,vel,enabled in source_notes:
                            a=max(pos,origin);b=min(pos+duration,origin+span)
                            if b>a:notes.append([a-origin,b-a,key,vel,0.,0.,0,not enabled])
                        plen=max(.25,span);pid=self.add_pattern(t,cname,plen,notes)
                        t['clips'].append(clip(start+elapsed,span,pid,name=cname,muted=muted));elapsed+=span;origin=loop_start;iterations+=1
                        if iterations>10000:raise ValueError('Too many loop repetitions in an ALS clip.')
                else:
                    file_ref=c.find('SampleRef/FileRef');path=self.resolve(value(file_ref,'Path'),value(file_ref,'RelativePath'));audio=self.audio(path)
                    warped=boolean(c,'IsWarped',False);offset=number(value(c,'Loop/LoopStart',0))+number(value(c,'Loop/StartRelative',0))
                    if warped:
                        markers=[(number(n.get('BeatTime')),number(n.get('SecTime'))) for n in c.findall('./WarpMarkers/WarpMarker')];markers.sort()
                        if len(markers)>=2:
                            a,b=markers[0],markers[1];offset=a[1]+(offset-a[0])*(b[1]-a[1])/max(1e-9,b[0]-a[0])
                        else:offset=offset*60/bpm
                        self.warn('Warp/time-stretch must be checked or rendered in the source host: '+cname)
                    if boolean(c,'Loop/LoopOn',False):self.warn('Audio looping must be checked: '+cname)
                    t['clips'].append(clip(start,length,offset=max(0,offset),name=cname,audio=audio,muted=muted or not audio))
                    if not audio:t['clips'][-1][8]='[Missing audio] '+cname
                    if number(value(c,'PitchCoarse',0)) or number(value(c,'PitchFine',0)):self.warn('Audio transposition must be checked: '+cname)
            if node.find('.//AutomationEnvelopes') is not None:self.warn('Automation envelopes require manual migration: '+name)
        for fid,parent in pending_groups:
            if parent in folders:self.project['folders'][fid]['parent']=folders[parent]
        self.warn('Import is a migration of arrangement/MIDI/audio. Devices, plugin states, returns, modulation, warping and tempo automation are not recreated. Choose instruments for imported MIDI tracks.')
    def flp(self):
        self.status('Reading FLP patterns and playlist',.05)
        if self.source.stat().st_size>MAX_BYTES:raise ValueError('FLP exceeds 512 MB.')
        data=self.source.read_bytes()
        if len(data)<22:raise ValueError('Truncated FLP header.')
        magic,size,fmt,channels,ppq=struct.unpack_from('<4sIhHH',data)
        if magic!=b'FLhd' or size!=6 or fmt!=0 or not 1<=ppq<=9600 or data[14:18]!=b'FLdt':raise ValueError('Unsupported or corrupt FLP header.')
        if struct.unpack_from('<I',data,18)[0]!=len(data)-22:raise ValueError('FLP data length is invalid.')
        patterns={};rack={};playlists=[];current_channel=None;current_pattern=None;version=(20,0);arrangement=0;at=22;track_names={};track_id=None
        modern_tempo=None;coarse_tempo=None;fine_tempo=None
        while at<len(data):
            eid=data[at];at+=1
            # FL 25's 0xAC has THREE payload bytes, despite its DWORD opcode.
            # Four-byte parsing consumes the next event ID and corrupts tempo,
            # channel names and notes. See flpdiff's FL25 format findings.
            if eid==172 and version>=(25,0):n=3
            elif eid<192:n=1 if eid<64 else 2 if eid<128 else 4
            else:
                n=0;shift=0
                while True:
                    if at>=len(data) or shift>28:raise ValueError('Invalid FLP event length.')
                    byte=data[at];at+=1;n|=(byte&127)<<shift;shift+=7
                    if not byte&128:break
            if n>len(data)-at:raise ValueError('Truncated FLP event.')
            raw=data[at:at+n];at+=n;integer=int.from_bytes(raw,'little') if eid<192 else 0
            def text():return raw.decode('utf-16-le' if version>=(11,5) else 'cp1252','replace').rstrip('\0')
            if eid==199:
                try:version=tuple(map(int,raw.decode('ascii').strip('\0').split('.')[:2]))
                except ValueError:raise ValueError('Invalid FLP version.')
            elif eid==156 and modern_tempo is None:modern_tempo=integer/1000
            elif eid==66 and coarse_tempo is None:coarse_tempo=integer
            elif eid==93 and fine_tempo is None:fine_tempo=integer
            elif eid==194:self.project['name']=text() or self.project['name']
            elif eid==64:current_channel=integer;rack.setdefault(integer,dict(name='Channel '+str(integer+1),type=0,root=60))
            elif eid==65:current_pattern=integer;patterns.setdefault(integer,dict(name='Pattern '+str(integer),notes={},length=0))
            elif eid==192 and current_channel is not None:rack[current_channel]['name']=text()
            elif eid==203 and current_channel is not None:rack[current_channel]['name']=text() or rack[current_channel]['name']
            elif eid==21 and current_channel is not None:rack[current_channel]['type']=integer
            elif eid==196 and current_channel is not None:rack[current_channel]['sample']=text()
            elif eid==135 and current_channel is not None:rack[current_channel]['root']=integer
            elif eid==98:current_channel=None
            elif eid==193 and current_pattern is not None:patterns[current_pattern]['name']=text()
            elif eid==164 and current_pattern is not None:patterns[current_pattern]['length']=integer/ppq
            elif eid==224 and current_pattern is not None:
                if len(raw)%24:raise ValueError('Unsupported FLP note record size.')
                p=patterns[current_pattern]
                for offset in range(0,len(raw),24):
                    pos,flags,channel,duration,key,group,fine,unknown,release,midi,pan,velocity,x,y=struct.unpack_from('<IHHIHH8B',raw,offset)
                    if key>127:self.warn('Notes above MIDI key 127 were omitted.');continue
                    note=[pos/ppq,max(duration/ppq,.25 if duration==0 else .00001),key,min(1,velocity/128),(pan-64)/64,(fine-120)*10,midi&15,False]
                    p['notes'].setdefault(channel,[]).append(note)
                    if flags&8:self.warn('Slide-note articulation must be recreated manually.')
            elif eid==99:arrangement+=1
            elif eid==233:
                if not raw:continue
                # Late FL24 and FL25 use 80 bytes. Several FL20.99 saves already
                # use 60. Validate EVERY core signature; divisibility alone is
                # ambiguous for 240-byte blobs (32/60/80).
                preferred=80 if version>=(24,2) else 60 if version>=(21,0) else 32
                candidates=[preferred]+[k for k in (32,60,80,88) if k!=preferred]
                record=next((k for k in candidates if len(raw)%k==0 and all(struct.unpack_from('<H',raw,o+4)[0]==20480 for o in range(0,len(raw),k))),None)
                if record is None:raise ValueError(f'Unrecognized FLP playlist layout ({len(raw)} bytes; file version {version[0]}.{version[1]}). No project was replaced.')
                items=[]
                for offset in range(0,len(raw),record):
                    pos,base,index,duration,row,group,unknown,flags,unused,start,end=struct.unpack_from('<IHHIHHHHIff',raw,offset)
                    items.append((pos/ppq,base,index,duration/ppq,row,start,end))
                playlists.append(items)
            elif eid==238 and len(raw)>=4:track_id=struct.unpack_from('<I',raw)[0]
            elif eid==239 and track_id is not None:track_names[track_id]=text()
        # Modern BPM * 1000 takes precedence regardless of event order. Older files
        # use separate whole-BPM and fractional events; placeholders must not
        # overwrite a valid modern value. See PyFLP Project.tempo.
        legacy_tempo=coarse_tempo+(fine_tempo or 0)/1000 if coarse_tempo is not None else None
        if modern_tempo is not None and MIN_TEMPO<=modern_tempo<=MAX_TEMPO:self.set_tempo(modern_tempo)
        elif legacy_tempo is not None and MIN_TEMPO<=legacy_tempo<=MAX_TEMPO:
            self.set_tempo(legacy_tempo)
            if modern_tempo is not None:self.warn(f'Invalid modern tempo ({modern_tempo:g} BPM); recovered legacy tempo {legacy_tempo:g} BPM.')
        elif modern_tempo is not None:self.set_tempo(modern_tempo)
        elif legacy_tempo is not None:self.set_tempo(legacy_tempo)
        else:self.warn('No project tempo event found; using 120 BPM. Set the correct tempo before playback.')
        if len(playlists)>1:self.warn('Only the first FLP arrangement was imported; other arrangements remain in the source project.')
        tmap={};pmap={};audio_rows={};used=set()
        def channel_track(channel):
            if channel not in tmap:
                c=rack.get(channel,dict(name='Channel '+str(channel+1),type=0));t=self.add_track(track(c['name']));tmap[channel]=t
                if c.get('sample'):
                    audio=self.audio(self.resolve(c['sample']))
                    if audio:t.update(sample=audio,sampler=True,chromatic=True,rootNote=min(96,c.get('root',60)),oneShot=False)
                    else:t['mute']=True
                if c.get('type',0) not in (0,4):self.instrument_warning(t,['source instrument / assign a VST'])
            return tmap[channel]
        def pattern_parts(channel,pid):
            key=(channel,pid)
            if key not in pmap:
                p=patterns[pid];notes=p['notes'].get(channel,[])
                length=p['length'] or max(4,math.ceil(max((n[0]+n[1] for ns in p['notes'].values() for n in ns),default=4)/4)*4)
                if length>16384:raise ValueError('FLP pattern exceeds 4096 bars.')
                parts=[];start=0.
                while start<length-1e-8:
                    span=min(256.,length-start);chunk=[]
                    for n in notes:
                        a=max(start,n[0]);b=min(start+span,n[0]+n[1])
                        if b>a:
                            copy=n.copy();copy[0]=a-start;copy[1]=b-a;chunk.append(copy)
                            if n[0]<start:self.warn('A sustained note crosses a split pattern boundary and is retriggered: '+p['name'])
                    name=p['name']+(f' / Part {len(parts)+1}' if length>256 else '')
                    parts.append((start,span,self.add_pattern(channel_track(channel),name,max(.25,span),chunk)))
                    start+=span
                pmap[key]=(length,parts)
                if length>256:self.warn('Long pattern split into editable parts, with playlist timing preserved: '+p['name'])
            return pmap[key]
        for at,base,index,length,row,start,end in playlists[0] if playlists else []:
            if index>base:
                pid=index-base
                if pid not in patterns:self.warn('Playlist references missing pattern '+str(pid));continue
                for channel in patterns[pid]['notes']:
                    t=channel_track(channel);period,parts=pattern_parts(channel,pid);offset=max(0,start/ppq) if math.isfinite(start) else 0
                    if len(parts)==1:
                        c=clip(at,length,parts[0][2],name=patterns[pid]['name']);c[7]=offset%period;t['clips'].append(c)
                    else:
                        elapsed=0.
                        while elapsed<length-1e-8:
                            phase=(offset+elapsed)%period;part=next((p for p in parts if p[0]<=phase<p[0]+p[1]),parts[0]);span=min(length-elapsed,part[0]+part[1]-phase)
                            if span<=1e-8:raise ValueError('Invalid FLP pattern repetition.')
                            c=clip(at+elapsed,span,part[2],name=patterns[pid]['name']);c[7]=phase-part[0];t['clips'].append(c);elapsed+=span
                    used.add(pid)
            else:
                channel=index;source=rack.get(channel,{})
                if source.get('type')==5:self.warn('Automation clips require manual migration.');continue
                file=self.resolve(source.get('sample'));audio=self.audio(file)
                # Multiple placements on a playlist lane belong to ONE track.
                # Creating a track per clip exhausted 512 on ordinary songs.
                if row not in audio_rows:audio_rows[row]=self.add_track(track(track_names.get(500-row,source.get('name','Audio '+str(channel+1)))))
                t=audio_rows[row]
                t['clips'].append(clip(at,length,offset=max(0,start) if math.isfinite(start) else 0,name=t['name'],audio=audio,muted=not audio))
                if not audio:t['clips'][-1][8]='[Missing audio] '+t['name']
        # Keep unplaced patterns editable, without inventing arrangement placements.
        for pid,p in patterns.items():
            for channel in p['notes']:pattern_parts(channel,pid)
        for channel,source in rack.items():
            if source.get('type')!=5 and channel not in tmap and (not playlists or source.get('type')!=4):channel_track(channel)
        if not self.project['tracks']:self.add_track(track('Track 1'))
        self.warn('FLP channels become editable VampDAW tracks. Source playlist lanes, mixer routing, plugin states, automation, channel envelopes and time-stretch are not recreated. Choose instruments for imported MIDI tracks.')
        if not playlists and patterns:self.warn('No supported playlist was found; patterns are available in Project Pool.')
    def run(self):
        self.output.mkdir(parents=True,exist_ok=True)
        if self.source.suffix.lower()=='.als':self.als()
        elif self.source.suffix.lower()=='.flp':self.flp()
        else:raise ValueError('Choose an .als or .flp project.')
        if not self.project['tracks']:raise ValueError('No supported tracks or patterns were found in this project.')
        self.set_tempo(self.project['bpm'])
        end=max((c[0]+c[1] for t in self.project['tracks'] for c in t['clips']),default=32)
        if end>16384:raise ValueError('Arrangement exceeds 4096 bars.')
        self.project['bars']=max(1,math.ceil(end/4));self.status('Writing editable project',.95)
        target=self.output/'Imported.vamp';target.write_text(json.dumps(self.project,ensure_ascii=False,allow_nan=False),encoding='utf-8')
        result=dict(passed=True,source=str(self.source),project=str(target),tracks=len(self.project['tracks']),clips=sum(len(t['clips']) for t in self.project['tracks']),warnings=self.warnings,missing_audio=self.missing,devices_to_assign=self.devices,source_modified=False)
        (self.output/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
        lines=['VampDAW / PROJECT IMPORT',self.source.name,'Tracks: '+str(result['tracks'])+' | Clips: '+str(result['clips']),'','Original file is unchanged. Save your work as a .vamp project.','','Review before playback:']+self.warnings
        (self.output/'Import Report.txt').write_text('\n'.join(lines),encoding='utf-8-sig');self.status('Import complete',1);return result

def main():
    p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--output',required=True);p.add_argument('--encoder',required=True);a=p.parse_args()
    try:Importer(a.source,a.output,a.encoder).run()
    except Exception as e:
        out=Path(a.output);out.mkdir(parents=True,exist_ok=True);(out/'report.json').write_text(json.dumps(dict(passed=False,error=str(e))),encoding='utf-8');raise SystemExit(1)
if __name__=='__main__':main()
