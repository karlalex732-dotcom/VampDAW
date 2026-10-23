"""Package only the built Oracle binaries, licenses, usage guide and corresponding source."""
from pathlib import Path
import hashlib,json,shutil,subprocess,tempfile,zipfile
ROOT=Path(__file__).resolve().parents[1]
def digest(path):
 h=hashlib.sha256()
 with Path(path).open('rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
def main():
 bundle=ROOT/'build/Oracle Rack.vst3';source=ROOT/'dist/VampDAW-0.4.15-source.zip'
 for p in (bundle/'Contents/x86_64-win/Oracle Rack.vst3',bundle/'Contents/x86_64-win/OracleScan.exe',source,ROOT/'docs/verification-0.4.15.json'):
  if not p.is_file():raise RuntimeError('Required verified artifact missing: '+str(p))
 stage=Path(tempfile.mkdtemp(prefix='oracle-release-',dir=ROOT/'build'))
 def copy(src,name):
  target=stage/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,target)
 for name in ('Oracle Rack.vst3','OracleScan.exe'):copy(bundle/'Contents/x86_64-win'/name,'Oracle Rack.vst3/Contents/x86_64-win/'+name)
 for name in ('LICENSE.txt','THIRD_PARTY_NOTICES.txt'):copy(ROOT/name,'Oracle Rack.vst3/'+name)
 copy(ROOT/'third_party/pluginterfaces/LICENSE.txt','Oracle Rack.vst3/licenses/VST3-MIT.txt')
 copy(ROOT/'third_party/json/LICENSE.MIT','Oracle Rack.vst3/licenses/JSON-MIT.txt')
 copy(ROOT/'docs/ORACLE-RACK.md','README.md');copy(ROOT/'docs/verification-0.4.15.json','Verification.json');copy(source,'Sources/'+source.name)
 manifest={'product':'Oracle Rack','version':'0.1.0-alpha','publisher':'Karsei','architecture':'x86_64-baseline','format':'VST3 bundle','hosted_formats':['Windows x64 VST2','Windows x64 VST3','Vamp'],'daw_required':False,'internal_processing':'float32','code_signed':False,'license':'GPL-3.0-only','files':{p.relative_to(stage).as_posix():{'sha256':digest(p),'bytes':p.stat().st_size} for p in sorted(stage.rglob('*')) if p.is_file()}}
 (stage/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
 output=ROOT/'dist/Oracle-Rack-0.1.0-win64.zip'
 with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for p in sorted(stage.rglob('*')):
   if p.is_file():z.write(p,p.relative_to(stage).as_posix(),compress_type=zipfile.ZIP_STORED if p.suffix=='.zip' else zipfile.ZIP_DEFLATED)
 subprocess.run([str(ROOT/'tools/innosetup/ISCC.exe'),'/DStage='+str(stage),str(ROOT/'packaging/Oracle-Rack.iss')],cwd=ROOT,check=True)
 installer=ROOT/'dist/Oracle-Rack-0.1.0-alpha-win64-Setup.exe';report={'stage':str(stage),'installer':str(installer),'installer_sha256':digest(installer),'zip':str(output),'zip_sha256':digest(output),'source_sha256':digest(source)}
 (ROOT/'build/last-oracle-stage.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if __name__=='__main__':main()
