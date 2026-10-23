#pragma once
#include "engine.hpp"
#include "sample_edit.hpp"
#include "stem_state.hpp"
#include <mutex>
#include <thread>

namespace vamp {
inline void writeStemInput(const fs::path&path,const Sample&sample,size_t first,size_t last,const std::atomic<bool>&cancel){
    if(last<=first||last>sample.left.size()||sample.right.size()!=sample.left.size()||(last-first)>((uint64_t)UINT32_MAX-36)/8)throw std::runtime_error("Invalid stem input range");
    std::ofstream file(path,std::ios::binary);auto u16=[&](uint16_t v){file.write((char*)&v,2);};auto u32=[&](uint32_t v){file.write((char*)&v,4);};uint32_t bytes=(uint32_t)((last-first)*8);file.write("RIFF",4);u32(bytes+36);file.write("WAVEfmt ",8);u32(16);u16(3);u16(2);u32(sample.rate);u32(sample.rate*8);u16(8);u16(32);file.write("data",4);u32(bytes);
    std::array<float,MaxBlock*2>block{};for(size_t at=first;at<last;at+=MaxBlock){if(cancel)throw std::runtime_error("Cancelled");size_t n=std::min<size_t>(MaxBlock,last-at);for(size_t i=0;i<n;i++){block[i*2]=sample.left[at+i];block[i*2+1]=sample.right[at+i];}file.write((char*)block.data(),n*8);if(!file)throw std::runtime_error("Cannot write stem input");}file.flush();if(!file)throw std::runtime_error("Cannot finish stem input");
}
inline std::wstring processArgument(const std::wstring&value){std::wstring out=L"\"";size_t slashes=0;for(wchar_t c:value){if(c==L'\\'){slashes++;continue;}if(c==L'\"')out.append(slashes*2+1,L'\\');else out.append(slashes,L'\\');slashes=0;out+=c;}out.append(slashes*2,L'\\');out+=L'\"';return out;}
class StemSeparator {
    std::thread worker;std::mutex mutex;std::wstring stage;Json result=Json::object();
    void update(float value,const std::wstring&text){progress=value;std::lock_guard<std::mutex>lock(mutex);stage=text;}
public:
    std::atomic<bool>running{false},done{false},cancel{false};std::atomic<float>progress{0};fs::path output;std::vector<std::shared_ptr<Sample>>samples;
    ~StemSeparator(){cancel=true;if(worker.joinable())worker.join();}
    std::wstring status(){std::lock_guard<std::mutex>lock(mutex);return stage;}
    Json takeResult(){if(worker.joinable())worker.join();done=false;std::lock_guard<std::mutex>lock(mutex);return result;}
    void start(const fs::path&python,const fs::path&script,const fs::path&directory,const fs::path&cache,Json options,std::shared_ptr<Sample>sample,const fs::path&file,double first,double last){
        if(running)throw std::runtime_error("Stem separation is already running");if(!fs::exists(python)||!fs::exists(script))throw std::runtime_error("Stem engine is missing. Install the VampDAW Stem Pack.");
        if(worker.joinable())worker.join();cancel=false;done=false;result=Json::object();samples.clear();output=directory;progress=0;update(0,L"Preparing audio");running=true;
        try{worker=std::thread([=]()mutable{
            HANDLE process=nullptr,processThread=nullptr,job=nullptr,log=nullptr,nul=nullptr;
            bool comReady=false;auto hostLog=directory/L"host.log";
            try{
                comReady=SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED));update(.0f,L"Preparing audio");fs::create_directories(directory);appendStemLog(hostLog,"prepare",utf8(python.wstring()));
                if(!sample)sample=readAudio(file);if(cancel)throw std::runtime_error("Cancelled");
                size_t begin=(size_t)clamp(std::floor(first*sample->rate),0,sample->left.size());size_t end=last>first?(size_t)clamp(std::round(last*sample->rate),begin,sample->left.size()):sample->left.size();if(end<=begin)throw std::runtime_error("Select nonempty audio to separate");
                auto input=directory/L"input.wav";writeStemInput(input,*sample,begin,end,cancel);sample.reset();
                auto jobFile=directory/L"job.json",statusFile=directory/L"status.json",resultFile=directory/L"result.json";
                options["input"]=utf8(input.wstring());options["output"]=utf8(directory.wstring());options["cache"]=utf8(cache.wstring());options["status"]=utf8(statusFile.wstring());options["result"]=utf8(resultFile.wstring());{std::ofstream f(jobFile);f<<options.dump(2);if(!f)throw std::runtime_error("Cannot write stem job");}
                SECURITY_ATTRIBUTES sa{sizeof sa,nullptr,TRUE};log=CreateFileW((directory/L"engine.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);nul=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
                if(log==INVALID_HANDLE_VALUE||nul==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open stem log");
                job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;if(!job||!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof limits))throw std::runtime_error("Cannot create stem worker job");
                STARTUPINFOW startup{sizeof startup};startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdOutput=startup.hStdError=log;startup.hStdInput=nul;PROCESS_INFORMATION info{};
                auto command=processArgument(python.wstring())+L" -I "+processArgument(script.wstring())+L" --job "+processArgument(jobFile.wstring());
                if(!CreateProcessW(python.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|BELOW_NORMAL_PRIORITY_CLASS,nullptr,directory.c_str(),&startup,&info))throw std::runtime_error("Cannot launch stem engine; Windows error "+std::to_string(GetLastError()));process=info.hProcess;processThread=info.hThread;
                if(!AssignProcessToJobObject(job,process)){TerminateProcess(process,1);throw std::runtime_error("Cannot isolate stem worker");}if(ResumeThread(processThread)==(DWORD)-1)throw std::runtime_error("Cannot start stem worker");CloseHandle(processThread);processThread=nullptr;
                appendStemLog(hostLog,"worker_started",std::to_string(info.dwProcessId));DWORD waitStatus;
                while((waitStatus=WaitForSingleObject(process,150))==WAIT_TIMEOUT){if(cancel){TerminateJobObject(job,1);WaitForSingleObject(process,5000);throw std::runtime_error("Cancelled");}try{Json value;std::ifstream f(statusFile);if(f){f>>value;update((float)clamp(value.value("progress",0.),0,1),wide(stemText(value,"stage","Separating audio")));}}catch(...){} }
                if(waitStatus!=WAIT_OBJECT_0)throw std::runtime_error("Cannot wait for stem engine");
                DWORD code=1;if(!GetExitCodeProcess(process,&code))throw std::runtime_error("Cannot read stem engine exit status");appendStemLog(hostLog,"worker_exit",stemExitMessage(code));
                if(code){std::string detail;try{std::ifstream f(resultFile);Json failed;if(f){f>>failed;detail=stemText(failed,"error");}}catch(...){}throw std::runtime_error(stemExitMessage(code)+(detail.empty()?"":". "+detail));}
                {std::ifstream f(resultFile);if(!f)throw std::runtime_error("Stem engine returned no result");if(fs::file_size(resultFile)>1024*1024)throw std::runtime_error("Stem engine response is too large");f>>result;}
                validateStemResult(result);if(!stemFlag(result,"passed"))throw std::runtime_error(stemText(result,"error","Stem separation failed"));
                update(.99f,L"Loading stems for preview");for(auto&source:result.at("sources")){if(cancel)throw std::runtime_error("Cancelled");samples.push_back(readAudio(wide(stemText(source,"import_path",stemText(source,"path")))));}
                appendStemLog(hostLog,"complete",std::to_string(samples.size()));update(1,L"Complete");
            }catch(const std::exception&e){samples.clear();std::string detail=std::string(e.what())+"\nLogs: "+utf8(directory.wstring());appendStemLog(hostLog,cancel?"cancelled":"error",detail);result={{"passed",false},{"cancelled",cancel.load()},{"error",detail}};update(0,cancel?L"Cancelled":L"Stem separation failed — see error details and logs");}
            catch(...){samples.clear();appendStemLog(hostLog,"error","Unknown stem worker error");result={{"passed",false},{"error","Unknown stem worker error"}};update(0,L"Stem separation failed — see error details and logs");}
            if(job)CloseHandle(job);if(processThread)CloseHandle(processThread);if(process)CloseHandle(process);if(log&&log!=INVALID_HANDLE_VALUE)CloseHandle(log);if(nul&&nul!=INVALID_HANDLE_VALUE)CloseHandle(nul);if(comReady)CoUninitialize();running=false;done=true;
        });}catch(...){running=false;done=false;throw;}
    }
};
}
