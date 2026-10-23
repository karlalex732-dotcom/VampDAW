#pragma once
#include "model.hpp"
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace vamp {
struct WaveformRange {
    float low=0,high=0;bool valid=false;
    void add(float value){if(!std::isfinite(value))return;if(!valid){low=high=value;valid=true;}else{low=std::min(low,value);high=std::max(high,value);}}
    void add(const WaveformRange&v){if(v.valid){add(v.low);add(v.high);}}
};
inline WaveformRange waveformFrames(const Sample&s,size_t first,size_t last){
    WaveformRange out;last=std::min(last,s.left.size());
    for(size_t i=first;i<last;i++){out.add(s.left[i]);if(i<s.right.size())out.add(s.right[i]);}return out;
}
// Immutable min/max levels preserve short transients at every horizontal zoom.
// Only 1/64 of the raw frames are retained in the first level.
struct PlaylistWaveform {
    static constexpr size_t block=64;
    std::vector<std::vector<WaveformRange>>levels;float peak=0;
    static std::shared_ptr<PlaylistWaveform> build(const Sample&s,const std::atomic<bool>*cancel=nullptr){
        auto out=std::make_shared<PlaylistWaveform>();if(s.left.empty())return out;
        out->levels.emplace_back();auto&base=out->levels.back();base.reserve((s.left.size()+block-1)/block);
        for(size_t i=0;i<s.left.size();i+=block){
            if((i%(block*1024)==0)&&cancel&&cancel->load())return {};
            auto range=waveformFrames(s,i,std::min(i+block,s.left.size()));base.push_back(range);
            if(range.valid)out->peak=std::max({out->peak,std::abs(range.low),std::abs(range.high)});
        }
        while(out->levels.back().size()>1){
            auto&old=out->levels.back();std::vector<WaveformRange>next;next.reserve((old.size()+3)/4);
            for(size_t i=0;i<old.size();i+=4){WaveformRange range;for(size_t j=i;j<std::min(i+4,old.size());j++)range.add(old[j]);next.push_back(range);}
            out->levels.push_back(std::move(next));
        }return out;
    }
    WaveformRange range(const Sample&s,size_t first,size_t last)const{
        WaveformRange out;last=std::min(last,s.left.size());first=std::min(first,last);
        if(levels.empty()||last-first<block*2)return waveformFrames(s,first,last);
        size_t aligned=std::min(last,((first+block-1)/block)*block);out.add(waveformFrames(s,first,aligned));first=aligned;
        while(last-first>=block){
            size_t level=0,span=block;
            while(level+1<levels.size()&&span<=((last-first)/4)&&first%(span*4)==0){level++;span*=4;}
            out.add(levels[level][first/span]);first+=span;
        }out.add(waveformFrames(s,first,last));return out;
    }
    size_t bytes()const{size_t n=0;for(auto&v:levels)n+=v.capacity()*sizeof(WaveformRange);return n;}
};
// Analysis never scans a whole file on the UI or audio thread. Entries own only
// peaks, while a weak sample reference lets closed projects release their audio.
class PlaylistWaveformCache {
    struct Entry{std::weak_ptr<Sample>sample;std::shared_ptr<const PlaylistWaveform>peaks;uint64_t used=0;};
    struct Job{std::shared_ptr<Sample>sample;HWND notify=nullptr;};
    std::mutex mutex;std::condition_variable wake;std::map<const Sample*,Entry>entries;std::deque<Job>queue;
    std::thread worker;std::atomic<bool>cancel{false};uint64_t clock=0;
    void trim(){
        size_t bytes=0;for(auto i=entries.begin();i!=entries.end();)if(i->second.sample.expired())i=entries.erase(i);else{if(i->second.peaks)bytes+=i->second.peaks->bytes();++i;}
        while(bytes>96ull*1024*1024){auto oldest=entries.end();for(auto i=entries.begin();i!=entries.end();++i)if(i->second.peaks&&(oldest==entries.end()||i->second.used<oldest->second.used))oldest=i;if(oldest==entries.end())break;bytes-=oldest->second.peaks->bytes();entries.erase(oldest);}
    }
    void run(){
        for(;;){Job job;{std::unique_lock<std::mutex>lock(mutex);wake.wait(lock,[&]{return cancel||!queue.empty();});if(cancel)return;job=std::move(queue.front());queue.pop_front();}
            std::shared_ptr<const PlaylistWaveform>peaks;try{peaks=PlaylistWaveform::build(*job.sample,&cancel);}catch(...){}
            {std::lock_guard<std::mutex>lock(mutex);auto entry=entries.find(job.sample.get());if(entry!=entries.end()){entry->second.peaks=std::move(peaks);entry->second.used=++clock;}trim();}
            if(!cancel&&job.notify&&IsWindow(job.notify))InvalidateRect(job.notify,nullptr,FALSE);
        }
    }
public:
    ~PlaylistWaveformCache(){stop();}
    void stop(){cancel=true;wake.notify_all();if(worker.joinable())worker.join();std::lock_guard<std::mutex>lock(mutex);queue.clear();entries.clear();}
    std::shared_ptr<const PlaylistWaveform>get(const std::shared_ptr<Sample>&sample,HWND notify=nullptr){
        if(!sample||sample->left.empty()||cancel)return {};
        std::lock_guard<std::mutex>lock(mutex);auto old=entries.find(sample.get());
        if(old!=entries.end()&&!old->second.sample.expired()){old->second.used=++clock;return old->second.peaks;}
        if(queue.size()>=32)return {};if(!worker.joinable())worker=std::thread([this]{run();});
        entries[sample.get()]={sample,{},++clock};queue.push_back({sample,notify});wake.notify_one();return {};
    }
};
inline WaveformRange playlistWaveformRange(const Sample&s,const PlaylistWaveform*peaks,double begin,double end){
    size_t first=(size_t)clamp(std::floor(begin),0,(double)s.left.size());
    size_t last=(size_t)clamp(std::ceil(end),0,(double)s.left.size());if(last<=first)return {};
    if(peaks)return peaks->range(s,first,last);
    if(last-first<=64)return waveformFrames(s,first,last);
    WaveformRange out;for(size_t i=0;i<32;i++){size_t n=first+(last-first-1)*i/31;out.add(s.left[n]);if(n<s.right.size())out.add(s.right[n]);}return out;
}
}
