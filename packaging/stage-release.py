"""Whitelist staging and corresponding source. Python 3.11+, standard library only."""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess, tempfile, zipfile

ROOT=Path(__file__).resolve().parents[1]
def digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as file:
        for block in iter(lambda:file.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--binary',type=Path,default=ROOT/'build/VampDAW-0.4.19.exe')
    binary=parser.parse_args().binary.resolve();encoder=ROOT/'tools/ffmpeg/bin/ffmpeg.exe'
    for path in (binary,encoder):
        if not path.is_file():raise RuntimeError(f'Build missing: {path}')
    dependencies=json.loads((ROOT/'packaging/dependencies.json').read_text())
    for name,entry in dependencies.items():
        if '.tar.' in name and digest(ROOT/'work/packaging'/name)!=entry['sha256']:raise RuntimeError('Source hash mismatch: '+name)
    stage=Path(tempfile.mkdtemp(prefix='installer-stage-',dir=ROOT/'build'))
    def copy(source,target):
        destination=stage/target;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/source,destination)
    copy(binary,'VampDAW.exe')
    copy('tools/ffmpeg/bin/ffmpeg.exe','tools/ffmpeg/bin/ffmpeg.exe')
    for name in ('LICENSE.txt','THIRD_PARTY_NOTICES.txt'):copy(name,name)
    copy('packaging/README-ALPHA.txt','README-ALPHA.txt')
    copy('packaging/Collect-DefenderReport.ps1','Collect-DefenderReport.ps1')
    documents=('KEYBOARD.md','NATIVE-EFFECTS.md','DSP-0.4.5.md','STEM-SEPARATOR.md','UPDATE-0.4.6.md','UPDATE-0.4.7.md','UPDATE-0.4.8.md','UPDATE-0.4.9.md','UPDATE-0.4.10.md','UPDATE-0.4.11.md','UPDATE-0.4.12.md','UPDATE-0.4.13.md','UPDATE-0.4.14.md','UPDATE-0.4.15.md','UPDATE-0.4.16.md','UPDATE-0.4.17.md','UPDATE-0.4.18.md','UPDATE-0.4.19.md','ARA.md','ORACLE-RACK.md','verification-0.4.19.json','NEXT-10-FEATURES.md')
    for name in documents:copy('docs/'+name,'docs/'+name)
    stem_root=ROOT/'tools/stems'
    if not (stem_root/'python/python.exe').is_file():raise RuntimeError('Prepare the portable Stem Engine before packaging')
    stem_sources=[p for p in stem_root.rglob('*') if p.is_file() and p.relative_to(stem_root).parts[0] not in {'python','models'} and '__pycache__' not in p.parts and p.suffix not in {'.pyc','.pyo'}]
    for path in stem_sources:copy(path,'tools/stems/'+path.relative_to(stem_root).as_posix())
    shutil.copytree(ROOT/'tools/project-import',stage/'tools/project-import',ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
    shutil.copytree(stem_root/'python',stage/'tools/stems/python',ignore=shutil.ignore_patterns('__pycache__','*.pyc','*.pyo','*.lib'))
    model_manifest={}
    for name in ('955717e8-8726e21a.th','5c90dfd2-34c22ccb.th'):
        path=stem_root/'models/hub/checkpoints'/name
        copy(path,'tools/stems/models/hub/checkpoints/'+name)
        model_manifest[name]={'sha256':digest(path),'bytes':path.stat().st_size,'source':'https://dl.fbaipublicfiles.com/demucs/hybrid_transformer/'+name,'license':'MIT / Demucs'}
    (stage/'tools/stems/bundled-models.json').write_text(json.dumps(model_manifest,indent=2))
    (stage/'VampDAW.installed').write_text('VampDAW installed mode / user data is stored outside this directory.\n')
    licenses={
      'ARA-Apache-2.0.txt':'third_party/ara/LICENSE.txt',
      'VST3-MIT.txt':'third_party/pluginterfaces/LICENSE.txt',
      'VeSTige-GPL3.txt':'LICENSE.txt',
      'JSON-MIT.txt':'third_party/json/LICENSE.MIT',
      'ASIO-SDK.txt':'third_party/asio/ASIOSDK/LICENSE.txt',
      'LAME-LGPL.txt':'work/codec-src/lame-3.100/COPYING',
      'Ogg-BSD.txt':'work/codec-src/libogg-1.3.6/COPYING',
      'Vorbis-BSD.txt':'work/codec-src/libvorbis-1.3.7/COPYING',
      'Opus-BSD.txt':'work/codec-src/opus-1.5.2/COPYING',
      'Zig-MIT.txt':'tools/zig-x86_64-windows-0.15.2/LICENSE',
      'LLVM-libcxx.txt':'tools/zig-x86_64-windows-0.15.2/lib/libcxx/LICENSE.TXT',
      'LLVM-libcxxabi.txt':'tools/zig-x86_64-windows-0.15.2/lib/libcxxabi/LICENSE.TXT',
      'LLVM-libunwind.txt':'tools/zig-x86_64-windows-0.15.2/lib/libunwind/LICENSE.TXT',
      'MinGW-Zig.txt':'tools/zig-x86_64-windows-0.15.2/lib/libc/mingw/COPYING',
      'MinGW-encoder.txt':'tools/w64devkit/COPYING.MinGW-w64-runtime.txt',
      'Inno-Setup.txt':'tools/innosetup/license.txt',
      'GCC-runtime-exception.txt':'packaging/GCC-runtime-exception.txt',
      'GCC-GPL3.txt':'LICENSE.txt',
      'Python.txt':'tools/stems/python/LICENSE.txt',
      'Demucs-MIT.txt':'tools/stems/python/Lib/site-packages/demucs-4.0.1.dist-info/licenses/LICENSE',
      'MSST-MIT.txt':'tools/stems/vendor/LICENSE'}
    for path in (ROOT/'work/codec-src/ffmpeg-8.1.3').glob('COPYING.*'):licenses['FFmpeg-'+path.name]=path.relative_to(ROOT).as_posix()
    licenses['FFmpeg-LICENSE.md']='work/codec-src/ffmpeg-8.1.3/LICENSE.md'
    for name,path in licenses.items():copy(path,'licenses/'+name)
    (ROOT/'dist').mkdir(exist_ok=True)
    source=ROOT/'dist/VampDAW-0.4.19-source.zip';prefix='VampDAW-0.4.19-source/';sources={}
    def add(path,name=None):sources[name or path.relative_to(ROOT).as_posix()]=path
    for folder in ('src','assets','tests','plugins','packaging','third_party/pluginterfaces','third_party/json','third_party/vestige','third_party/ara','tools/project-import'):
        for path in (ROOT/folder).rglob('*'):
            if path.is_file() and not any(p in {'__pycache__','.git'} for p in path.parts):add(path)
    for name in documents:add(ROOT/'docs'/name)
    for path in stem_sources:add(path)
    for name in ('iasiodrv.h','asio.h','asiosys.h'):add(ROOT/'third_party/asio/ASIOSDK/common'/name)
    add(ROOT/'third_party/asio/ASIOSDK/LICENSE.txt')
    for name in ('build.ps1','build-codecs.ps1','build-installer.ps1','build-oracle.ps1','LICENSE.txt','THIRD_PARTY_NOTICES.txt'):add(ROOT/name)
    add(ROOT/'packaging/SOURCE-BUILD.txt','README.txt')
    for path in (stage/'licenses').iterdir():add(path,'licenses/'+path.name)
    for name in dependencies:
        if '.tar.' in name:add(ROOT/'work/packaging'/name,'dependency-sources/'+name)
    with zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
        for name,path in sorted(sources.items()):archive.write(path,prefix+name,compress_type=zipfile.ZIP_STORED if '.tar.' in name else zipfile.ZIP_DEFLATED)
        archive.writestr(prefix+'SOURCE-SHA256.json',json.dumps({n:digest(p) for n,p in sorted(sources.items())},indent=2))
    copy(source.relative_to(ROOT).as_posix(),'Sources/'+source.name)
    config=subprocess.run([str(encoder),'-version'],capture_output=True,text=True,check=True).stdout
    manifest={'product':'VampDAW','version':'0.4.19-alpha','ara_api':'2.3.0 / VST3 / experimental; commercial plug-ins not tested','architecture':'x86_64-baseline','windows_minimum':'11 (22000)','license':'GPL-3.0-only','offline':True,'optional_model_downloads':['htdemucs_ft','wind specialists'],'stem_runtime':'CPython 3.12 / PyTorch 2.5.1 CPU / Demucs 4.0.1','encoder_build':config,'runtime_exe_extraction':False,'code_signed':False,'files':{p.relative_to(stage).as_posix():{'sha256':digest(p),'bytes':p.stat().st_size} for p in sorted(stage.rglob('*')) if p.is_file()}}
    (stage/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps({'stage':str(stage),'source':str(source),'source_bytes':source.stat().st_size,'application_sha256':digest(binary),'encoder_bytes':encoder.stat().st_size}))

if __name__=='__main__':main()
