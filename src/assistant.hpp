#pragma once
#include "model.hpp"
#include <functional>
namespace vamp {
struct AssistantPreset {
    const char*name;bool mastering=false,vocal=false;
    double highpass=30,mud=-1,air=0,ratio=2,attack=20,release=180,target=-20,reverb=0,delay=0,drive=0,width=100;
};
const std::vector<AssistantPreset>&assistantPresets();
struct AssistantAnalysis {
    bool valid=false;double seconds=0,peak=-120,rms=-120,crest=0,noise=-120,dc=0,correlation=1;
    double sub=0,bass=0,mud=0,presence=0,air=0,centroid=0,clipped=0;
};
struct AssistantStep {int kind=0;std::string reason;std::vector<std::pair<int,double>>settings;std::string path,name;bool bypass=false;};
struct AssistantPlan {int preset=0;bool analyzed=false;AssistantAnalysis analysis;std::vector<AssistantStep>steps;double strength=65;};
AssistantAnalysis analyzeAssistant(const float*l,const float*r,size_t frames,double rate);
int detectAssistantPreset(const AssistantAnalysis&,bool master);
AssistantPlan makeAssistantPlan(int preset,double strength,const AssistantAnalysis*analysis=nullptr);
// Audio thread writes only during capture; UI reads only after the release-published Complete state.
struct AssistantCapture {
    std::vector<float>left,right;double rate;size_t written=0;bool started=false;
    std::atomic<int>state{0};std::atomic<size_t>progress{0},targetFrames{0};
    explicit AssistantCapture(double sampleRate):left((size_t)std::ceil(sampleRate*30)),right(left.size()),rate(sampleRate){targetFrames=(size_t)std::ceil(rate*4);}
    bool request(double seconds=4){int expected=0;if(!state.compare_exchange_strong(expected,4))return false;seconds=std::isfinite(seconds)?clamp(seconds,1,30):4;targetFrames=(size_t)std::ceil(rate*seconds);progress=0;state.store(1,std::memory_order_release);return true;}
    void process(const float*l,const float*r,int frames);
};
}
