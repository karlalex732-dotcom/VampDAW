"""Create the relocatable CPU Stem Pack from the pinned build environment.
Run with the same CPython base used to create tools/stem-build-env.
"""
from pathlib import Path
import importlib.metadata
import json
import shutil
import sys
import subprocess
root = Path(__file__).resolve().parents[1]
destination = root / 'tools/stems/python'
base = Path(sys.base_prefix)
destination.mkdir(parents=True, exist_ok=True)
for name in ('python.exe', 'python312.dll', 'python3.dll', 'vcruntime140.dll', 'vcruntime140_1.dll', 'LICENSE.txt'):
    shutil.copy2(base/name, destination/name)
shutil.copytree(base/'DLLs', destination/'DLLs', dirs_exist_ok=True, ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
def ignore(directory, names):
    skip = {'__pycache__', 'site-packages', 'test', 'tests', 'idlelib', 'tkinter', 'turtledemo', 'ensurepip'}
    return [name for name in names if name in skip or name.endswith(('.pyc', '.pyo'))]
shutil.copytree(base/'Lib', destination/'Lib', dirs_exist_ok=True, ignore=ignore)
packages = root/'tools/stem-build-env/Lib/site-packages'
shutil.copytree(packages, destination/'Lib/site-packages', dirs_exist_ok=True,
                ignore=shutil.ignore_patterns('__pycache__', '*.pyc', '*.pyo', '*.lib', 'pip', 'pip-*', 'tests'))
requirements = subprocess.check_output([str(root/'tools/stem-build-env/Scripts/python.exe'), '-m', 'pip', 'freeze'], text=True)
for path in destination.rglob('*.lib'):
    if not path.resolve().is_relative_to(destination.resolve()):
        raise RuntimeError('Unexpected runtime path')
    path.unlink()  # Static developer libraries are not used by inference DLLs.
(root/'tools/stems/requirements-lock.txt').write_text(requirements, encoding='utf-8')
print(json.dumps({'python': str(destination/'python.exe'), 'bytes': sum(p.stat().st_size for p in destination.rglob('*') if p.is_file())}))
