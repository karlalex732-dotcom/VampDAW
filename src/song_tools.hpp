#pragma once
#include "model.hpp"
#include "sample_edit.hpp"
#include <deque>
#include <map>
namespace vamp {
inline Groove extractGroove(const std::vector<Note>&notes,double length,double grid=.25){
    Groove g;g.length=clamp(length,.25,64);g.grid=clamp(grid,1./32,4);std::map<int,std::vector<const Note*>>cells;
    for(auto&n:notes)if(!n.muted&&n.beat>=0&&n.beat<g.length)cells[(int)std::llround(n.beat/g.grid)].push_back(&n);
    for(auto&cell:cells){double offset=0,velocity=0;for(auto*n:cell.second){offset+=n->beat-cell.first*g.grid;velocity+=n->velocity;}g.hits.push_back({std::fmod(cell.first*g.grid,g.length),clamp(offset/cell.second.size(),-g.grid*.49,g.grid*.49),(float)(velocity/cell.second.size())});}
    if(g.hits.empty())throw std::runtime_error("The source has no rhythm to extract");return g;
}
inline Groove extractAudioGroove(const Sample&s,double bpm,double grid=.25){auto slices=sliceSample(s,32,true);std::vector<Note>notes;for(auto&slice:slices){size_t a=(size_t)(slice.start*s.rate),b=std::min(s.left.size(),a+(size_t)(s.rate*.04));float peak=0;for(size_t i=a;i<b;i++)peak=std::max(peak,std::max(std::abs(s.left[i]),std::abs(s.right[i])));if(peak>.01)notes.push_back({slice.start*bpm/60,.05,60,peak});}return extractGroove(notes,std::min(64.,s.left.size()/double(s.rate)*bpm/60),grid);}
inline std::vector<Note> applyGroove(const std::vector<Note>&notes,const Groove&g,double timing,double dynamics,double length){
    if(g.hits.empty()||g.grid<=0||g.length<=0)throw std::runtime_error("Empty groove");auto out=notes;timing=clamp(timing,0,1);dynamics=clamp(dynamics,0,1);
    for(auto&n:out){double cell=std::round(n.beat/g.grid)*g.grid,local=std::fmod(cell,g.length);auto best=g.hits.end();double distance=1e9;for(auto i=g.hits.begin();i!=g.hits.end();i++){double d=std::abs(i->beat-local);d=std::min(d,g.length-d);if(d<distance){distance=d;best=i;}}if(best==g.hits.end()||distance>g.grid*.51)continue;double target=cell+best->offset;n.beat=clamp(n.beat+(target-n.beat)*timing,0,std::max(0.,length-.001));n.length=std::min(n.length,length-n.beat);n.velocity=(float)clamp(n.velocity+(best->velocity-n.velocity)*dynamics,0,1);}
    std::stable_sort(out.begin(),out.end(),[](auto&a,auto&b){return a.beat<b.beat;});return out;
}
inline std::shared_ptr<Project> versionSnapshot(const Project&p){auto snapshot=std::make_shared<Project>(p);snapshot->versions.clear();snapshot->abWorking.reset();for(auto&t:snapshot->tracks){t.armed=t.monitor=false;for(auto&c:t.clips){c.warpAudio.reset();c.warpKey=0;}}return snapshot;}
inline void storeVersion(Project&p,const std::string&name){if(name.empty()||p.versions.size()>=16)throw std::runtime_error("Maximum 16 named versions per project");p.versions.push_back({name,versionSnapshot(p)});}
inline Project recallVersion(const Project&p,int index){if(index<0||index>=(int)p.versions.size()||!p.versions[index].snapshot)throw std::runtime_error("Choose a saved version");Project out=*p.versions[index].snapshot;out.versions=p.versions;out.abWorking=p.abWorking;return out;}
uint64_t warpSignature(const Sample&s,const Clip&clip,const TempoMap&clock);
std::shared_ptr<Sample> renderWarp(const Sample&source,const Clip&clip,const TempoMap&clock);
struct RecallMidiEvent {uint32_t message=0;int track=0;double seconds=0;};
class MidiRecall {
    std::deque<RecallMidiEvent>events;
public:
    bool enabled=false;
    void clear(){events.clear();}
    void push(RecallMidiEvent e){if(!enabled)return;events.push_back(e);while(!events.empty()&&(events.size()>32768||e.seconds-events.front().seconds>30))events.pop_front();}
    std::vector<Note> recover(int track,double now,double seconds,double bpm)const{
        if(!enabled)throw std::runtime_error("Enable retrospective MIDI before playing");seconds=clamp(seconds,1,30);double first=now-seconds;std::map<int,RecallMidiEvent>held;std::vector<Note>notes;
        auto finish=[&](int key,double end){auto i=held.find(key);if(i==held.end())return;auto e=i->second;held.erase(i);double begin=std::max(first,e.seconds);if(end<=begin)return;Note n;n.beat=(begin-first)*bpm/60;n.length=std::max(.01,(end-begin)*bpm/60);n.pitch=(e.message>>8)&127;n.channel=e.message&15;n.velocity=((e.message>>16)&127)/127.f;notes.push_back(n);};
        for(auto&e:events)if(e.track==track&&e.seconds<=now){int status=e.message&0xf0,pitch=(e.message>>8)&127,key=(e.message&15)*128+pitch;if(status==0x90&&((e.message>>16)&127)){finish(key,e.seconds);held[key]=e;}else if(status==0x80||status==0x90)finish(key,e.seconds);else if(status==0xb0&&pitch==123){while(!held.empty())finish(held.begin()->first,e.seconds);}}
        while(!held.empty())finish(held.begin()->first,now);std::stable_sort(notes.begin(),notes.end(),[](auto&a,auto&b){return a.beat<b.beat;});return notes;
    }
};
}
