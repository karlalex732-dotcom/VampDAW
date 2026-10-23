#include "stem_separator.hpp"
static void stemWorkerTests(){
    const fs::path python=fs::absolute(L"tools/stems/python/python.exe"),script=fs::absolute(L"tools/stems/separate.py"),cache=fs::absolute(L"tools/stems/models");
    if(!fs::exists(python)||!fs::exists(cache/L"hub/checkpoints/955717e8-8726e21a.th")){std::ofstream(L"work/test-results/stem-worker.json")<<Json({{"skipped",true},{"reason","Optional Stem Pack not present"}}).dump(2);return;}
    auto source=std::make_shared<Sample>();source->rate=48000;source->left.resize(24000);source->right.resize(24000);for(int i=0;i<24000;i++)source->left[i]=source->right[i]=(float)std::sin(i*.021)*1.5f;
    Json options={{"model","htdemucs"},{"target","all"},{"shifts",0},{"segment",2},{"threads",2},{"encoding","FLOAT"}};
    auto directory=fs::absolute(L"work/test-results/stems/native-"+std::to_wstring(GetTickCount64()));StemSeparator job;job.start(python,script,directory,cache,options,source,{},0,.25);auto deadline=GetTickCount64()+120000;while(!job.done&&GetTickCount64()<deadline)Sleep(20);if(!job.done){job.cancel=true;throw std::runtime_error("Stem worker timeout");}auto result=job.takeResult();check(result.value("passed",false)&&job.samples.size()==4,"native DAW worker launches real model and loads four stems");for(auto&sample:job.samples)check(sample->rate==48000&&sample->left.size()==12000,"stem input range, frame count and sample rate are preserved");
    auto snapshot=readWav(directory/L"input.wav");for(size_t i=0;i<12000;i++)check(snapshot->left[i]==source->left[i]&&snapshot->right[i]==source->right[i],"stem float input preserves samples above 0 dBFS without clipping or quantization");
    job.start(python,script,directory/L"cancelled",cache,options,source,{},0,0);Sleep(100);job.cancel=true;deadline=GetTickCount64()+10000;while(!job.done&&GetTickCount64()<deadline)Sleep(20);check(job.done,"cancel terminates the isolated worker promptly");auto cancelled=job.takeResult();check(cancelled.value("cancelled",false)&&!cancelled.value("passed",true),"cancel is not reported as a successful separation");
    check(processArgument(L"C:\\Audio Files\\")==L"\"C:\\Audio Files\\\\\"","native process arguments preserve spaces and trailing slash");
    auto fixture=directory/L"fixture.py";{std::ofstream f(fixture);f<<R"PY(import json,sys,pathlib,ctypes
job=json.loads(pathlib.Path(sys.argv[2]).read_text())
mode=job['fixture']
if mode=='fatal':ctypes.windll.kernel32.ExitProcess(ctypes.c_uint32(0xC000001D))
if mode=='missing_dll':ctypes.windll.kernel32.ExitProcess(ctypes.c_uint32(0xC0000135))
if mode=='no_result':sys.exit(0)
result=None
if mode=='bad_sources':result={'passed':True,'sources':{'name':'not an array'}}
if mode=='partial_preview':result={'passed':True,'sources':[{'name':'First','path':job['input']},{'name':'Missing','path':str(pathlib.Path(job['output'])/'absent.wav')}]}
if mode=='good':result={'passed':True,'sources':[{'name':'Fixture','path':job['input']}]}
pathlib.Path(job['result']).write_text(json.dumps(result))
)PY";}
    Json failures=Json::array();for(const char*mode:{"fatal","missing_dll","no_result","null","bad_sources","partial_preview","good"}){
        Json config=options;config["fixture"]=mode;auto folder=directory/wide(mode);job.start(python,fixture,folder,cache,config,source,{},0,.1);
        deadline=GetTickCount64()+15000;while(!job.done&&GetTickCount64()<deadline)Sleep(10);check(job.done,"failure fixture returns control to the host");auto value=job.takeResult();validateStemResult(value);
        if(std::string(mode)=="good")check(stemFlag(value,"passed")&&job.samples.size()==1,"same host recovers after worker crashes and invalid responses");
        else{check(!stemFlag(value,"passed")&&job.samples.empty(),"worker failure is reported without exposing partial previews");check(fs::exists(folder/L"host.log"),"failure diagnostic log exists");failures.push_back({{"mode",mode},{"error",stemText(value,"error")}});}
        if(std::string(mode)=="fatal")check(stemText(value,"error").find("0xC000001D")!=std::string::npos,"CPU failure preserves process exit code");
        if(std::string(mode)=="missing_dll")check(stemText(value,"error").find("DLL")!=std::string::npos,"missing runtime DLL produces a useful message");
    }
    bool missing=false;try{job.start(directory/L"absent.exe",fixture,directory/L"missing-runtime",cache,options,source,{},0,0);}catch(const std::exception&){missing=true;}check(missing&&!job.running,"missing optional runtime does not wedge the host");
    std::ofstream(L"work/test-results/stem-failures.json")<<Json({{"passed",true},{"isolated_worker_failures",failures},{"host_survived_and_recovered",true},{"partial_previews_cleared",true},{"missing_runtime_caught",missing}}).dump(2);
    std::ofstream(L"work/test-results/stem-worker.json")<<Json({{"passed",true},{"real_cpu_model",true},{"native_process_launch",true},{"loaded_sources",4},{"selected_range_frames",12000},{"float_snapshot_lossless",true},{"cancel_isolated_worker",true}}).dump(2);
}
