"""Migration acceptance checks: source bytes remain unchanged; imported MIDI and
audio are compiled by the actual native host, not only by the parser."""
from pathlib import Path
import gzip,hashlib,json,struct,subprocess,sys,wave
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'work/test-results/project-import';OUT.mkdir(parents=True,exist_ok=True)
# Exercise the Windows sharing race between the native poller and atomic status.
import ctypes,importlib.util,threading,time
spec=importlib.util.spec_from_file_location('vamp_project_import',ROOT/'tools/project-import/import_project.py');adapter=importlib.util.module_from_spec(spec);spec.loader.exec_module(adapter)
status_out=OUT/'locked-progress';status_out.mkdir(exist_ok=True);imp=adapter.Importer(OUT/'unread.als',status_out,ROOT/'tools/ffmpeg/bin/ffmpeg.exe');imp.status('First',0)
k32=ctypes.WinDLL('kernel32',use_last_error=True);k32.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p];k32.CreateFileW.restype=ctypes.c_void_p;k32.CloseHandle.argtypes=[ctypes.c_void_p]
handle=k32.CreateFileW(str(status_out/'status.json'),0x80000000,3,None,3,0x80,None);assert handle not in (None,ctypes.c_void_p(-1).value)
release=threading.Thread(target=lambda:(time.sleep(.09),k32.CloseHandle(handle)));release.start();assert imp.status('After transient lock',.5);release.join();assert json.loads((status_out/'status.json').read_text())['progress']==.5
replace=adapter.os.replace
def blocked(*_):raise PermissionError('Simulated persistent status reader')
adapter.os.replace=blocked
try:assert imp.status('Optional status',.9) is False
finally:adapter.os.replace=replace
assert json.loads((status_out/'status.json').read_text())['progress']==.5
sample=OUT/'sample.wav'
with wave.open(str(sample),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(48000);f.writeframes(b'\0\x20'*48000)
def scalar(tag,v):return f'<{tag} Value="{v}"/>'
midi='''<MidiClip Time="8"><CurrentStart Value="8"/><CurrentEnd Value="16"/><Name Value="Lead"/><Loop><LoopStart Value="0"/><LoopEnd Value="4"/><StartRelative Value="0"/><LoopOn Value="true"/></Loop><Notes><KeyTracks><KeyTrack Id="0"><MidiKey Value="64"/><Notes><MidiNoteEvent Time="1" Duration="0.5" Velocity="100" IsEnabled="true"/></Notes></KeyTrack></KeyTracks></Notes></MidiClip>'''
audio=f'''<AudioClip Time="20"><CurrentStart Value="20"/><CurrentEnd Value="22"/><Name Value="Take"/><IsWarped Value="false"/><Loop><LoopStart Value="0"/><LoopOn Value="false"/></Loop><SampleRef><FileRef><RelativePath Value="sample.wav"/></FileRef></SampleRef></AudioClip>'''
def als_track(tag,name,events):return f'<{tag} Id="1"><Name><UserName Value="{name}"/></Name><DeviceChain><Mixer><Volume><Manual Value="0.75"/></Volume><Pan><Manual Value="0.25"/></Pan></Mixer><MainSequencer><ClipTimeable><ArrangerAutomation><Events>{events}</Events></ArrangerAutomation></ClipTimeable></MainSequencer></DeviceChain></{tag}>'
xml='<Ableton><LiveSet><MainTrack><DeviceChain><Mixer><Tempo><Manual Value="138"/></Tempo></Mixer></DeviceChain></MainTrack><Tracks>'+als_track('MidiTrack','Synth',midi)+als_track('AudioTrack','Guitar',audio)+'</Tracks></LiveSet></Ableton>'
als=OUT/'Fixture.als';als.write_bytes(gzip.compress(xml.encode()))
def varint(n):
    out=bytearray()
    while n>=128:out.append((n&127)|128);n>>=7
    return bytes(out)+bytes([n])
def ev(i,v):
    if i<192:return bytes([i])+v.to_bytes(1 if i<64 else 2 if i<128 else 4,'little')
    return bytes([i])+varint(len(v))+v
def text(s):return (s+'\0').encode('utf-16-le')
def flp_file(version,record,tempo_events=None):
    note=struct.pack('<IHHIHH8B',96,0,0,48,64,0,120,0,64,0,64,100,128,128)
    item=struct.pack('<IHHIHHHHIff',768,20480,20481,768,499,0,120,64,0,0,0)
    if record==60:item+=b'\0'*28
    data=ev(199,version.encode()+b'\0')+(ev(156,138000) if tempo_events is None else tempo_events)+ev(64,0)+ev(21,2)+ev(192,text('Synth'))+ev(65,1)+ev(224,note)+ev(65,1)+ev(193,text('Lead'))+ev(99,1)+ev(233,item)
    return struct.pack('<4sIhHH',b'FLhd',6,0,1,96)+b'FLdt'+struct.pack('<I',len(data))+data
flp=OUT/'Fixture.flp';flp.write_bytes(flp_file('21.0.0',60))
old=OUT/'Fixture20.flp';old.write_bytes(flp_file('20.9.2',32))
binary=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'build/VampDAW-0.4.9-ready.exe'
for source in [als,flp,old]:
    before=hashlib.sha256(source.read_bytes()).hexdigest();folder=OUT/source.stem
    result=subprocess.run([str(binary),'--import-project-smoke',str(source),str(folder)],timeout=120,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    assert result.returncode==0,(source,result.returncode)
    report=json.loads((folder/'host-check.json').read_text());p=json.loads((folder/'Imported.vamp').read_text())
    assert report['passed'] and report['compiled_events']>=4 and p['bpm']==138
    assert hashlib.sha256(source.read_bytes()).hexdigest()==before
    if source.suffix=='.als':
        assert len(p['tracks'])==2 and p['tracks'][0]['notes'][0][:4]==[1,.5,64,100/127]
        assert [c[0] for c in p['tracks'][0]['clips']]==[8,12]
        assert Path(p['tracks'][1]['clips'][0][11]).is_file()
    else:assert p['tracks'][0]['notes'][0][:4]==[1,.5,64,100/128] and p['tracks'][0]['clips'][0][0]==8
# Modern tempo must take priority over legacy placeholders, in either order.
tempo_cases=[('modern-first',ev(156,138375)+ev(66,0),138.375),('modern-last',ev(66,0)+ev(156,138375),138.375),('legacy-conflict',ev(156,138375)+ev(66,120)+ev(93,500),138.375),('legacy-fraction',ev(66,138)+ev(93,375),138.375),('legacy-reordered',ev(93,375)+ev(66,138),138.375),('recovered-legacy',ev(156,0)+ev(66,160)+ev(93,250),160.25),('first-modern',ev(156,140000)+ev(156,95000),140.),('minimum',ev(156,10000),10.),('low',ev(156,29500),29.5),('above-300',ev(156,300500),300.5),('modern-maximum',ev(156,522000),522.),('legacy-maximum',ev(66,999),999.),('maximum',ev(156,999000),999.),('missing-tempo',b'',120.)]
for name,events,bpm in tempo_cases:
    source=OUT/('Tempo-'+name+'.flp');source.write_bytes(flp_file('21.0.0',60,events));before=hashlib.sha256(source.read_bytes()).hexdigest();folder=OUT/('Tempo-'+name)
    result=subprocess.run([str(binary),'--import-project-smoke',str(source),str(folder)],timeout=120,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0));assert result.returncode==0,name
    imported=json.loads((folder/'Imported.vamp').read_text());host=json.loads((folder/'host-check.json').read_text());assert imported['bpm']==bpm and host['bpm']==bpm and host['passed'],(name,imported['bpm'],host)
    assert hashlib.sha256(source.read_bytes()).hexdigest()==before
    if name in ('recovered-legacy','missing-tempo'):assert json.loads((folder/'report.json').read_text())['warnings']
for name,events,expected in [('zero',ev(156,0),'0 BPM'),('too-high',ev(156,1000000),'1000 BPM')]:
    source=OUT/('Tempo-invalid-'+name+'.flp');source.write_bytes(flp_file('21.0.0',60,events));folder=OUT/('Tempo-invalid-'+name)
    result=subprocess.run([str(ROOT/'tools/stems/python/python.exe'),'-B','-I',str(ROOT/'tools/project-import/import_project.py'),'--source',str(source),'--output',str(folder),'--encoder',str(ROOT/'tools/ffmpeg/bin/ffmpeg.exe')],timeout=30)
    error=json.loads((folder/'report.json').read_text());assert result.returncode!=0 and not error['passed'] and expected in error['error']
for bpm in (20.,330.5):
    source=OUT/('Tempo-ALS-'+str(bpm)+'.als');source.write_bytes(gzip.compress(xml.replace('Value="138"',f'Value="{bpm}"').encode()));folder=OUT/source.stem
    result=subprocess.run([str(binary),'--import-project-smoke',str(source),str(folder)],timeout=120,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0));assert result.returncode==0
    host=json.loads((folder/'host-check.json').read_text());assert host['passed'] and host['bpm']==bpm
# Corrupt inputs fail in the worker without touching the DAW or source.
bad=OUT/'Truncated.flp';bad.write_bytes(flp.read_bytes()[:-3]);folder=OUT/'rejected'
process=subprocess.run([str(ROOT/'tools/stems/python/python.exe'),'-B','-I',str(ROOT/'tools/project-import/import_project.py'),'--source',str(bad),'--output',str(folder),'--encoder',str(ROOT/'tools/ffmpeg/bin/ffmpeg.exe')],timeout=30)
assert process.returncode!=0 and not json.loads((folder/'report.json').read_text())['passed']
report=dict(passed=True,tempo_cases=len(tempo_cases),tempo_priority_and_legacy_fraction=True,tempo_10_to_999_preserved=True,invalid_tempo_reports_actual_bpm=True,windows_status_sharing_retry=True,locked_progress_nonfatal=True,als_gzip_xml=True,flp_20_32byte_playlist=True,flp_21_60byte_playlist=True,midi_pitch_velocity_timing=True,looped_placements=True,relative_audio_decoded=True,native_project_compile=True,source_unchanged=True,corrupt_input_rejected=True,real_user_projects_tested=False)
(ROOT/'work/test-results/project-import.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
