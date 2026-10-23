"""Device policy without CUDA hardware; imports no ML dependencies."""
from pathlib import Path
import importlib.util,json,types
r=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('stem_worker',r/'tools/stems/separate.py');worker=importlib.util.module_from_spec(spec);spec.loader.exec_module(worker)
def torch_stub(available,cuda_version):
    return types.SimpleNamespace(cuda=types.SimpleNamespace(is_available=lambda:available),version=types.SimpleNamespace(cuda=cuda_version))
cases=[]
for requested,available,version,expected,fallback in [('cpu',True,'12.4','cpu',False),('auto',True,'12.4','cuda',False),('cuda',True,'12.4','cuda',False),('auto',False,None,'cpu',False),('cuda',False,None,'cpu',True),('cuda',False,'12.4','cpu',True)]:
    device,used_fallback,note=worker.select_device(torch_stub(available,version),requested)
    assert (device,used_fallback)==(expected,fallback)
    if fallback:assert note
    cases.append({'requested':requested,'cuda_available':available,'cuda_build':version,'actual':device,'fallback':used_fallback})
failed=types.SimpleNamespace(cuda=types.SimpleNamespace(is_available=lambda:(_ for _ in ()).throw(RuntimeError('Driver unavailable'))),version=types.SimpleNamespace(cuda='12.4'))
assert worker.select_device(failed,'cuda')[0]=='cpu'
try:worker.select_device(torch_stub(True,'12.4'),'invalid')
except ValueError:pass
else:raise AssertionError('invalid device must be rejected')
out=r/'work/test-results/stem-device-policy.json';out.parent.mkdir(exist_ok=True,parents=True);out.write_text(json.dumps({'passed':True,'cases':cases,'driver_query_failure_falls_back':True,'physical_cuda_gpu_tested':False},indent=2));print(out)
