#pragma once
#include <windows.h>
#include <atomic>
#include <thread>
#include <array>
#include <algorithm>
namespace vamp {
inline constexpr UINT FramePulseMessage=WM_APP+35;
inline constexpr std::array<int,8>UiFrameLimits{60,90,120,144,165,180,210,240};
inline int normalizedFrameLimit(int value){value=std::clamp(value,60,240);return *std::min_element(UiFrameLimits.begin(),UiFrameLimits.end(),[&](int a,int b){return std::abs(a-value)<std::abs(b-value);});}
// High-resolution one-shot pacing; idle windows sleep, and at most one pulse is queued.
class FramePacer {
    HANDLE stopEvent=nullptr,wakeEvent=nullptr,timer=nullptr;std::thread worker;HWND target=nullptr;
    std::atomic<bool>continuous{false},requested{false},posted{false};std::atomic<int>fps{60};std::atomic<LONGLONG>lastPaint{0};LONGLONG frequency=1;
    static LONGLONG now(){LARGE_INTEGER value;QueryPerformanceCounter(&value);return value.QuadPart;}
public:
    ~FramePacer(){stop();}
    bool running()const{return worker.joinable();}
    int limit()const{return fps.load();}
    void stop(){if(worker.joinable()){SetEvent(stopEvent);worker.join();}if(timer)CloseHandle(timer);if(wakeEvent)CloseHandle(wakeEvent);if(stopEvent)CloseHandle(stopEvent);timer=wakeEvent=stopEvent=nullptr;target=nullptr;continuous=requested=posted=false;lastPaint=0;}
    void setLimit(int value){fps=normalizedFrameLimit(value);if(wakeEvent)SetEvent(wakeEvent);}
    void setContinuous(bool enabled){if(continuous.exchange(enabled)!=enabled&&wakeEvent)SetEvent(wakeEvent);}
    void request(){requested=true;if(wakeEvent)SetEvent(wakeEvent);}
    void acknowledge(){posted=false;if(wakeEvent)SetEvent(wakeEvent);}
    bool allowPaint(){if(!running())return true;auto tick=now(),period=(frequency+fps.load()-1)/fps.load();if(lastPaint&&tick-lastPaint<period){request();return false;}lastPaint=tick;return true;}
    bool start(HWND window,int value){stop();target=window;fps=normalizedFrameLimit(value);LARGE_INTEGER f;QueryPerformanceFrequency(&f);frequency=f.QuadPart;
        stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);wakeEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);timer=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_ALL_ACCESS);if(!timer)timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);if(!stopEvent||!wakeEvent||!timer){stop();return false;}
        try{worker=std::thread([this]{HANDLE idle[]{stopEvent,wakeEvent},wait[]{stopEvent,wakeEvent,timer};
            for(;;){if(posted||(!continuous&&!requested)){if(WaitForMultipleObjects(2,idle,FALSE,INFINITE)==WAIT_OBJECT_0)break;continue;}
                auto period=(frequency+fps.load()-1)/fps.load(),remaining=std::max<LONGLONG>(1,lastPaint.load()+period-now());LARGE_INTEGER due;due.QuadPart=-std::max<LONGLONG>(1,(remaining*10000000+frequency-1)/frequency);SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE);
                auto state=WaitForMultipleObjects(3,wait,FALSE,INFINITE);if(state==WAIT_OBJECT_0)break;if(state!=WAIT_OBJECT_0+2)continue;
                if(!continuous&&!requested)continue;requested=false;if(!posted.exchange(true)&&!PostMessageW(target,FramePulseMessage,0,0))posted=false;
            }
        });}catch(...){stop();return false;}return true;
    }
};
}
