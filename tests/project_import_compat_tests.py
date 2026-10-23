"""FLP format regressions using native host; optional local projects stay read-only.
No copyrighted project/sample data is included in the source distribution.
"""
from pathlib import Path
import hashlib,importlib.util,json,struct,subprocess,sys,wave
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'work/test-results/project-import-compat';OUT.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('adapter',ROOT/'tools/project-import/import_project.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
binary=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'build/VampDAW-0.4.10-ready.exe'
def ev(i,v):
    if i<192:return bytes([i])+v.to_bytes(1 if i<64 else 2 if i<128 else 4,'little')
    n=len(v);b=bytearray()
    while n>=128:b.append((n&127)|128);n>>=7
    return bytes([i])+b+bytes([n])+v
def text(s):return (s+'\0').encode('utf-16-le')
def note(pos=0,duration=48,channel=0):return struct.pack('<IHHIHH8B',pos,0,channel,duration,64,0,120,0,64,0,64,100,128,128)
def item(stride=80,pos=0,index=20481,duration=384,row=499,start=-1.):return struct.pack('<IHHIHHHHIff',pos,20480,index,duration,row,0,120,64,0,start,-1.)+bytes(stride-32)
def project(events):return struct.pack('<4sIhHH',b'FLhd',6,0,1,96)+b'FLdt'+struct.pack('<I',len(events))+events
def fixture(version='25.2.5.5319',stride=80,count=3,extra=b'',notes=None):
    prefix=ev(199,version.encode()+b'\0')+(bytes([172,1,1,0])+ev(192,text('Source version banner')) if version.startswith('25') else b'')
    return project(prefix+ev(156,140500)+ev(64,0)+ev(21,2)+ev(203,text('Named Synth'))+ev(65,1)+ev(224,note() if notes is None else notes)+extra+ev(99,0)+ev(233,b''.join(item(stride,i*384) for i in range(count))))
def native(source,name):
    before=hashlib.sha256(source.read_bytes()).hexdigest();dest=OUT/name
    result=subprocess.run([str(binary),'--import-project-smoke',str(source),str(dest)],timeout=180,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0));host=json.loads((dest/'host-check.json').read_text());assert result.returncode==0 and host['passed'],(name,host)
    assert hashlib.sha256(source.read_bytes()).hexdigest()==before,'Source changed'
    return json.loads((dest/'Imported.vamp').read_text()),host
cases=[]
for version,stride,count in [('25.2.5.5319',80,3),('24.2.99.4831',80,3),('20.99.3000.3136',60,4),('20.9.0',32,15),('25.2.5.5319',88,3)]:
    name='stride-'+version+'-'+str(stride);source=OUT/(name+'.flp');source.write_bytes(fixture(version,stride,count));p,h=native(source,name)
    assert p['bpm']==140.5 and len(p['tracks'])==1 and len(p['tracks'][0]['clips'])==count and p['tracks'][0]['name']=='Named Synth';cases.append(name)
source=OUT/'Long.flp';source.write_bytes(fixture(extra=ev(164,1024*96),count=1,notes=note(1023*96)+note(255*96,2*96)))
p,h=native(source,'long');t=p['tracks'][0];assert len(t['patterns'])==3 and any(n[0]==255 for n in t['patterns'][-1]['notes']);assert any('retriggered' in w for w in h['warnings']);cases.append('long-pattern-notes-retained')
# A 280-beat placement loops through the two source parts with an 8-beat offset.
events=ev(199,b'25.2.5.5319\0')+bytes([172,1,1,0])+ev(156,120000)+ev(64,0)+ev(21,2)+ev(65,1)+ev(224,note(270*96))+ev(164,272*96)+ev(233,item(80,384,20481,280*96,start=8*96))
source=OUT/'Offset.flp';source.write_bytes(project(events));p,h=native(source,'offset');clips=p['tracks'][0]['clips'];assert [c[0] for c in clips]==[4,252,268] and [c[1] for c in clips]==[248,16,16] and clips[0][7]==8;cases.append('long-pattern-loop-offset')
sample=OUT/'sample.wav'
with wave.open(str(sample),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(44100);f.writeframes(b'\0\x10'*4410)
def audio_source(name,path):
    source=OUT/(name+'.flp');events=ev(199,b'25.2.5.5319\0')+bytes([172,1,1,0])+ev(156,120000)+ev(64,0)+ev(21,4)+ev(203,text('Audio source'))+ev(196,text(str(path)))+ev(233,b''.join(item(80,pos=i*24,index=0,duration=24) for i in range(600)));source.write_bytes(project(events));return source
p,h=native(audio_source('Repeated',sample),'repeated');assert len(p['tracks'])==1 and len(p['tracks'][0]['clips'])==600 and p['tracks'][0]['clips'][0][11];cases.append('600-audio-clips-one-track')
broken=OUT/'broken.wav';broken.write_bytes(b'not audio');p,h=native(audio_source('Broken',broken),'broken');assert len(p['tracks'])==1 and all(c[10] for c in p['tracks'][0]['clips']) and any('could not be decoded' in w for w in h['warnings']);cases.append('unreadable-audio-retains-muted-placements')
local=[]
for index,arg in enumerate(sys.argv[2:]):
    source=Path(arg);p,h=native(source,'local-'+str(index));local.append(dict(name=source.name,tracks=len(p['tracks']),clips=h['clips'],bpm=h['bpm'],compiled_events=h['compiled_events'],missing_audio=len(h['missing_audio']),source_unchanged=True))
corpus=[];base=Path(r'C:\Program Files\Image-Line\FL Studio 2025\Data')
if base.exists():
    for source in base.rglob('*.flp'):
        before=hashlib.sha256(source.read_bytes()).hexdigest();imp=m.Importer(source,OUT/'corpus',ROOT/'tools/ffmpeg/bin/ffmpeg.exe');imp.status=lambda *_:None;imp.audio=lambda _:'';imp.flp();assert imp.project['tracks'];assert hashlib.sha256(source.read_bytes()).hexdigest()==before
        corpus.append(dict(name=source.name,tracks=len(imp.project['tracks'])))
report=dict(passed=True,regressions=cases,native_host_compile=True,local_projects=local,installed_corpus_parser_only=len(corpus),corpus_audio_decoding_tested=False,source_files_unchanged=True)
(ROOT/'work/test-results/project-import-compat.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report,ensure_ascii=False))
