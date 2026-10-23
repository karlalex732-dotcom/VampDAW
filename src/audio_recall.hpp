#pragma once
#include "model.hpp"
namespace vamp {
// Atomics avoid a data race with the callback while the UI copies the circular buffer.
class AudioRecall {
    std::unique_ptr<std::atomic<float>[]>left,right;size_t capacity=0,maximum=0;int sampleRate=48000;
    std::atomic<uint64_t>written{0};
public:
    std::atomic<bool>enabled{false};std::atomic<int>pair{0},mode{0},asioPair{0};
    void configure(int rate,int seconds=30){enabled=false;maximum=(size_t)rate*(int)clamp(seconds,1,30);capacity=maximum+MaxBlock*2;sampleRate=rate;left=std::make_unique<std::atomic<float>[]>(capacity);right=std::make_unique<std::atomic<float>[]>(capacity);written=0;enabled=true;}
    void clear(){written=0;}
    double available()const{return enabled?std::min<uint64_t>(written.load(),maximum)/double(sampleRate):0;}
    int rate()const{return sampleRate;}
    void push(const float*l,const float*r,int frames){if(!enabled||!capacity)return;auto start=written.load(std::memory_order_relaxed);for(int i=0;i<frames;i++){size_t at=(start+i)%capacity;left[at].store(l?l[i]:0,std::memory_order_relaxed);right[at].store(r?r[i]:l?l[i]:0,std::memory_order_relaxed);}written.store(start+frames,std::memory_order_release);}
    std::shared_ptr<Sample>recover(double seconds)const{if(!enabled||!capacity)throw std::runtime_error("Enable retrospective audio before playing");for(int retry=0;retry<3;retry++){uint64_t end=written.load(std::memory_order_acquire),n=std::min<uint64_t>({end,maximum,(uint64_t)(clamp(seconds,1,30)*sampleRate)});if(!n)throw std::runtime_error("The retrospective audio buffer is empty");auto out=std::make_shared<Sample>();out->rate=sampleRate;out->left.resize(n);out->right.resize(n);for(uint64_t i=0;i<n;i++){size_t at=(end-n+i)%capacity;out->left[i]=left[at].load(std::memory_order_relaxed);out->right[i]=right[at].load(std::memory_order_relaxed);}if(written.load(std::memory_order_acquire)-(end-n)<=capacity)return out;}throw std::runtime_error("The input changed while copying; retry retrospective capture");}
};
}
