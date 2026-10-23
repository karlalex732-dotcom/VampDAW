#include "editing.hpp"
#include <map>
#include <tuple>
#include <stdexcept>
namespace vamp {
void validateTrackFolders(const Project&p){
    if(p.folders.size()>1024)throw std::runtime_error("Maximum 1024 folders");
    std::map<int,int>parents;for(auto&f:p.folders)if(f.parent< -1||f.id<0||f.id>1000000||!parents.emplace(f.id,f.parent).second)throw std::runtime_error("Invalid or duplicate folder ID");
    for(auto&f:p.folders){int id=f.id;std::set<int>seen;while(id>=0){if(!seen.insert(id).second)throw std::runtime_error("A folder cannot contain itself");if(seen.size()>32)throw std::runtime_error("Maximum 32 folder levels");auto it=parents.find(id);if(it==parents.end())throw std::runtime_error("Folder parent does not exist");id=it->second;}}
    for(auto&t:p.tracks)if(t.folder< -1||(t.folder!=-1&&!parents.count(t.folder)))throw std::runtime_error("Track folder does not exist");
}
int createTrackFolder(Project&p,const std::string&name,int parent){
    auto next=p;int id=0;for(auto&f:p.folders)id=std::max(id,f.id+1);next.folders.push_back({id,parent,name.empty()?"Folder":name,false});validateTrackFolders(next);p=std::move(next);return id;
}
void assignTracksToFolder(Project&p,const std::set<int>&tracks,int folder){
    auto next=p;for(int i:tracks){if(i<0||i>=(int)next.tracks.size())throw std::runtime_error("Invalid selected track");next.tracks[i].folder=folder;}validateTrackFolders(next);p=std::move(next);
}
void moveTrackFolder(Project&p,int folder,int parent){
    auto next=p;auto it=std::find_if(next.folders.begin(),next.folders.end(),[&](auto&f){return f.id==folder;});if(it==next.folders.end())throw std::runtime_error("Folder does not exist");it->parent=parent;validateTrackFolders(next);p=std::move(next);
}
void dissolveTrackFolder(Project&p,int folder){
    auto next=p;auto it=std::find_if(next.folders.begin(),next.folders.end(),[&](auto&f){return f.id==folder;});if(it==next.folders.end())throw std::runtime_error("Folder does not exist");int parent=it->parent;
    for(auto&t:next.tracks)if(t.folder==folder)t.folder=parent;for(auto&f:next.folders)if(f.parent==folder)f.parent=parent;next.folders.erase(it);validateTrackFolders(next);p=std::move(next);
}
void swapPluginSlots(Track&t,int first,int second){
    if(first<0||second<0||first>=(int)t.pluginPaths.size()||second>=(int)t.pluginPaths.size())throw std::runtime_error("Effect slot outside channel rack");
    if(first==second)return;
    t.pluginStates.resize(t.pluginPaths.size());t.pluginBypass.resize(t.pluginPaths.size(),false);
    std::swap(t.pluginPaths[first],t.pluginPaths[second]);std::swap(t.pluginStates[first],t.pluginStates[second]);
    bool bypass=t.pluginBypass[first];t.pluginBypass[first]=t.pluginBypass[second];t.pluginBypass[second]=bypass;
    auto remap=[&](int slot){return slot==first?second:slot==second?first:slot;};
    auto nameSlot=[](std::string&name,int slot){auto end=name.find(" / ");if(end==std::string::npos||!end)return;for(size_t i=0;i<end;i++)if(name[i]<'0'||name[i]>'9')return;name=std::to_string(slot+1)+name.substr(end);};
    if(t.automationTarget==2){t.automationSlot=remap(t.automationSlot);nameSlot(t.automationName,t.automationSlot);}
    for(auto&lane:t.automationLanes)if(lane.target==2){lane.slot=remap(lane.slot);nameSlot(lane.name,lane.slot);}
}
std::pair<double,double> playlistTimeRange(double anchor,double end,int bars,double grid){
    double length=std::max(1,bars)*4.;grid=clamp(grid,.015625,length);
    double a=clamp(std::min(anchor,end),0,length),b=clamp(std::max(anchor,end),0,length);
    double first=std::min(length-grid,std::floor(a/grid+1e-9)*grid);
    double last=std::min(length,std::max(first+grid,std::ceil(b/grid-1e-9)*grid));
    return{first,last};
}
ClipSelection clipsInRectangle(const Project&p,double beatA,double beatB,double rowA,double rowB){
    ClipSelection result;if(p.tracks.empty())return result;double first=std::min(beatA,beatB),last=std::max(beatA,beatB);
    int top=(int)clamp(std::floor(std::min(rowA,rowB)),0,p.tracks.size()-1),bottom=(int)clamp(std::floor(std::max(rowA,rowB)),0,p.tracks.size()-1);
    for(int row=top;row<=bottom;row++)for(int i=0;i<(int)p.tracks[row].clips.size();i++){auto&clip=p.tracks[row].clips[i];if(clip.beat+clip.length>=first&&clip.beat<=last)result.insert({row,i});}return result;
}
int cloneTrack(Project&p,int index){
    if(index<0||index>=(int)p.tracks.size()||p.tracks.size()>=TrackCount)throw std::runtime_error("Cannot clone this track: maximum 512 tracks");
    auto copy=p.tracks[index];copy.name+=" / Copy";copy.armed=false;copy.monitor=false;copy.solo=false;
    p.tracks.push_back(std::move(copy));return (int)p.tracks.size()-1;
}
void reorderTrack(Project&p,int from,int to){
    int count=(int)p.tracks.size();if(from<0||to<0||from>=count||to>=count)throw std::runtime_error("Track position outside project");if(from==to)return;
    auto remap=[&](int target){if(target==MasterChannel)return target;if(target==from)return to;if(from<to&&target>from&&target<=to)return target-1;if(from>to&&target>=to&&target<from)return target+1;return target;};
    Track moved=std::move(p.tracks[from]);p.tracks.erase(p.tracks.begin()+from);p.tracks.insert(p.tracks.begin()+to,std::move(moved));
    auto fix=[&](Track&t){t.output=remap(t.output);for(auto&s:t.sends)s.target=remap(s.target);};for(auto&t:p.tracks)fix(t);fix(p.master);
}
void extendPattern(Project&p,int track,int pattern,double end){
    auto&t=p.tracks.at(track);double&length=pattern?t.patterns.at(pattern-1).length:t.patternLength;double before=length;
    length=std::min(256.,std::max(length,std::ceil(end/4)*4));if(length<=before)return;
    for(auto&clip:t.clips)if(clip.pattern==pattern&&clip.patternOffset==0&&std::abs(clip.length-before)<1e-7){clip.length=length;p.bars=std::max(p.bars,barsForEnd(clip.beat+clip.length));}
}
ClipSelection transformClips(Project&p,const ClipSelection&selection,double delta,int trackDelta,bool copy){
    struct Item{int track,pattern;Clip clip;Track source;};std::vector<Item>items;int highest=-1;
    for(auto&index:selection){if(index.first<0||index.first>=(int)p.tracks.size()||index.second<0||index.second>=(int)p.tracks[index.first].clips.size())throw std::runtime_error("Clip selection is no longer valid");
        auto&t=p.tracks[index.first];if(t.frozen)throw std::runtime_error("Unfreeze the track before editing its clips");auto clip=t.clips[index.second];int target=index.first+trackDelta;
        if(target<0||target>=TrackCount||clip.beat+delta<0||clip.beat+delta+clip.length>16384+1e-7)throw std::runtime_error("Clip move exceeds project bounds");
        // Keep only the relevant source pattern and sampler metadata in the snapshot.
        Track source;source.sample=t.sample;source.sampler=t.sampler;source.chromatic=t.chromatic;source.oneShot=t.oneShot;source.rootNote=t.rootNote;source.slices=t.slices;source.sampleBpm=t.sampleBpm;source.sampleKey=t.sampleKey;source.name=t.name;
        if(target!=index.first&&(!t.sample||t.sampler)){source.patternLength=patternLength(t,clip.pattern);source.notes=patternNotes(t,clip.pattern);}
        items.push_back({index.first,clip.pattern,clip,std::move(source)});highest=std::max(highest,target);
    }
    if(items.empty())return {};
    Project candidate=p;while(highest>=(int)candidate.tracks.size())appendTrack(candidate);
    if(!copy)for(auto i=selection.rbegin();i!=selection.rend();i++)candidate.tracks[i->first].clips.erase(candidate.tracks[i->first].clips.begin()+i->second);
    std::map<std::tuple<int,int,int>,int>patterns;ClipSelection result;
    for(auto&item:items){int destination=item.track+trackDelta;auto&t=candidate.tracks[destination];if(t.frozen)throw std::runtime_error("Unfreeze the destination track first");auto&source=item.source;auto clip=item.clip;clip.beat+=delta;
        if(trackDelta!=0&&!clip.audio){
            bool empty=t.clips.empty();
            if(source.sample){
                if(!empty&&(t.sample!=source.sample||t.sampler!=source.sampler))throw std::runtime_error("Move audio to an empty track or a track using the same sample");
                if(source.sampler&&!empty){bool same=t.chromatic==source.chromatic&&t.rootNote==source.rootNote&&t.oneShot==source.oneShot&&t.slices.size()==source.slices.size();for(size_t i=0;same&&i<t.slices.size();i++){auto&a=t.slices[i];auto&b=source.slices[i];same=a.start==b.start&&a.end==b.end&&a.semitones==b.semitones&&a.gain==b.gain&&a.reverse==b.reverse;}if(!same)throw std::runtime_error("Sampler pads differ: use an empty track");}
                if(empty){t.sample=source.sample;t.sampler=source.sampler;t.chromatic=source.chromatic;t.oneShot=source.oneShot;t.rootNote=source.rootNote;t.slices=source.slices;t.sampleBpm=source.sampleBpm;t.sampleKey=source.sampleKey;}
            }else if(t.sample)throw std::runtime_error("Move MIDI to an instrument track");
            if(!source.sample||source.sampler){auto key=std::make_tuple(item.track,item.pattern,destination);auto found=patterns.find(key);if(found==patterns.end()){
                if(t.patterns.size()>=255)throw std::runtime_error("Maximum 256 patterns per track");
                t.patterns.push_back({source.name+" / Pattern",source.patternLength,source.notes});clip.pattern=(int)t.patterns.size();patterns[key]=clip.pattern;
            }else clip.pattern=found->second;}
        }
        if(t.clips.size()>=10000)throw std::runtime_error("Maximum 10000 clips per track");t.clips.push_back(clip);result.insert({destination,(int)t.clips.size()-1});candidate.bars=std::max(candidate.bars,barsForEnd(clip.beat+clip.length));
    }
    p=std::move(candidate);return result;
}
}
#include "timeline_edit.inc"
