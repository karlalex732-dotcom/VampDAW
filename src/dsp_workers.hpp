#pragma once
#include <windows.h>
#include <avrt.h>
#include <objbase.h>
#include <xmmintrin.h>
#include <array>
#include <atomic>
#include <thread>
#include <vector>
#include <memory>
namespace vamp {
inline int availableDspProcessors(){DWORD n=GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);return (int)std::max<DWORD>(1,n?n:std::thread::hardware_concurrency());}
inline int resolvedDspProcessors(int requested,int available){return requested<=0?std::max(1,available):std::clamp(requested,1,std::max(1,available));}
// Created/reconfigured while the engine is stopped. No allocation or thread creation in render().
class DspWorkers {
    struct Worker {HANDLE start=nullptr,done=nullptr;std::thread thread;};
    std::vector<std::unique_ptr<Worker>>workers;int count=0;std::atomic<bool>quit{false};
    std::atomic<int>next{0};std::atomic<unsigned>generation{0},failures{0};int jobs=0;void*context=nullptr;void(*task)(void*,int)=nullptr;
    void drain(){generation.load(std::memory_order_acquire);for(;;){int n=next.fetch_add(1,std::memory_order_relaxed);if(n>=jobs)return;try{task(context,n);}catch(...){failures.fetch_add(1,std::memory_order_relaxed);}}}
public:
    ~DspWorkers(){stop();}
    int size()const{return count+1;}
    unsigned faults()const{return failures.load();}
    void stop(){quit=true;for(auto&w:workers)SetEvent(w->start);for(auto&w:workers){w->thread.join();CloseHandle(w->start);CloseHandle(w->done);}workers.clear();count=0;quit=false;}
    void configure(int requested){int total=resolvedDspProcessors(requested,availableDspProcessors());if(total==size())return;stop();try{workers.reserve(total-1);}catch(...){return;}
        for(int i=1;i<total;i++){auto w=std::make_unique<Worker>();w->start=CreateEventW(nullptr,FALSE,FALSE,nullptr);w->done=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!w->start||!w->done){if(w->start)CloseHandle(w->start);if(w->done)CloseHandle(w->done);break;}
            try{auto*slot=w.get();w->thread=std::thread([this,slot]{CoInitializeEx(nullptr,COINIT_MULTITHREADED);_mm_setcsr(_mm_getcsr()|0x8040);DWORD index=0;HANDLE mm=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);while(WaitForSingleObject(slot->start,INFINITE)==WAIT_OBJECT_0){if(quit)break;drain();SetEvent(slot->done);}if(mm)AvRevertMmThreadCharacteristics(mm);CoUninitialize();});workers.push_back(std::move(w));count++;}catch(...){CloseHandle(w->start);CloseHandle(w->done);break;}
        }
    }
    void run(int n,void*data,void(*fn)(void*,int)){if(n<2||!count){for(int i=0;i<n;i++)fn(data,i);return;}jobs=n;context=data;task=fn;next=0;generation.fetch_add(1,std::memory_order_release);int awake=std::min(count,n-1);for(int i=0;i<awake;i++)SetEvent(workers[i]->start);drain();
        // Each worker is already running. Individual waits avoid the 64-handle limit.
        for(int i=0;i<awake;i++)WaitForSingleObject(workers[i]->done,INFINITE);
    }
};
}
