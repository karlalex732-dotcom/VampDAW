#pragma once
#include <map>
#include "editing.hpp"
#include <cwctype>
namespace vamp {
struct PoolItem{int track=0,pattern=0;std::shared_ptr<Sample>audio;std::wstring name;double length=0;int uses=0;};
inline std::vector<PoolItem>projectPoolItems(const Project&project,bool audio,const std::wstring&filter={}){
    auto fold=[](std::wstring text){for(auto&c:text)c=std::towlower(c);return text;};auto search=fold(filter);std::vector<PoolItem>items;std::set<std::wstring>seen;std::set<const Sample*>pointers;
    for(size_t i=0;i<project.tracks.size();i++){auto&t=project.tracks[i];auto add=[&](PoolItem item){if(search.empty()||fold(item.name+L" "+wide(t.name)).find(search)!=std::wstring::npos)items.push_back(std::move(item));};
        if(!audio){add({(int)i,0,{},wide(t.patternName),t.patternLength});for(size_t n=0;n<t.patterns.size();n++)add({(int)i,(int)n+1,{},wide(t.patterns[n].name),t.patterns[n].length});}
        else{auto asset=[&](const std::shared_ptr<Sample>&sample){if(!sample)return;auto name=sample->path.empty()?wide(t.name):sample->path.filename().wstring();if(!search.empty()&&fold(name+L" "+wide(t.name)).find(search)==std::wstring::npos)return;auto key=fold(sample->path.lexically_normal().wstring());if(!key.empty()){if(!seen.insert(key).second)return;}else if(!pointers.insert(sample.get()).second)return;add({(int)i,0,sample,sample->path.empty()?wide(t.name):sample->path.filename().wstring(),sample->left.size()/double(sample->rate)});};asset(t.sample);for(auto&clip:t.clips)asset(clip.audio);asset(t.freezeAudio);}
    }
    std::map<std::pair<int,int>,int>patterns;std::map<std::wstring,int>files;std::map<const Sample*,int>memory;
    for(size_t t=0;t<project.tracks.size();t++)for(auto&clip:project.tracks[t].clips){if(clip.audio){if(clip.audio->path.empty())memory[clip.audio.get()]++;else files[fold(clip.audio->path.lexically_normal().wstring())]++;}else patterns[{(int)t,clip.pattern}]++;}
    for(auto&item:items)item.uses=item.audio?(item.audio->path.empty()?memory[item.audio.get()]:files[fold(item.audio->path.lexically_normal().wstring())]):patterns[{item.track,item.pattern}];
    return items;
}
inline int placePoolItem(Project&project,const PoolItem&item,int target,double beat){
    if(target<0||target>=(int)project.tracks.size()||!std::isfinite(beat)||beat<0)throw std::runtime_error("Choose a Playlist track and a valid position");auto&t=project.tracks[target];if(t.frozen)throw std::runtime_error("Unfreeze the track before adding clips");
    Clip clip;clip.beat=beat;if(item.audio){clip.audio=item.audio;clip.length=audioBeatLength(project,beat,item.audio->left.size()/double(item.audio->rate));clip.name=utf8(item.name);}
    else{if(item.track<0||item.track>=(int)project.tracks.size())throw std::runtime_error("Pattern is no longer available");auto&source=project.tracks[item.track];if(item.pattern<0||item.pattern>(int)source.patterns.size())throw std::runtime_error("Pattern is no longer available");if(target==item.track){clip.pattern=item.pattern;clip.length=item.pattern?source.patterns[item.pattern-1].length:source.patternLength;}else{if(t.patterns.size()>=255)throw std::runtime_error("Maximum 256 patterns per track");Pattern pattern=item.pattern?source.patterns[item.pattern-1]:Pattern{source.patternName,source.patternLength,source.notes};t.patterns.push_back(std::move(pattern));clip.pattern=(int)t.patterns.size();clip.length=t.patterns.back().length;}}
    if(clip.length<=0||beat+clip.length>16384)throw std::runtime_error("Clip is outside the song range");t.clips.push_back(std::move(clip));project.bars=std::max(project.bars,barsForEnd(t.clips.back().beat+t.clips.back().length));return (int)t.clips.size()-1;
}
inline bool samePoolAudio(const std::shared_ptr<Sample>&a,const std::shared_ptr<Sample>&b){
    if(!a||!b)return false;if(a==b)return true;if(a->path.empty()||b->path.empty())return false;
    auto x=a->path.lexically_normal().wstring(),y=b->path.lexically_normal().wstring();return !_wcsicmp(x.c_str(),y.c_str());
}
inline std::vector<std::pair<int,int>>poolOccurrences(const Project&project,const PoolItem&item){
    std::vector<std::pair<int,int>>out;for(int t=0;t<(int)project.tracks.size();t++)for(int c=0;c<(int)project.tracks[t].clips.size();c++){
        auto&clip=project.tracks[t].clips[c];if(item.audio?samePoolAudio(clip.audio,item.audio):t==item.track&&!clip.audio&&clip.pattern==item.pattern)out.push_back({t,c});
    }return out;
}
inline void sortPoolItems(std::vector<PoolItem>&items,const Project&project,int column,bool descending){
    auto folded=[](std::wstring s){for(auto&ch:s)ch=std::towlower(ch);return s;};
    std::stable_sort(items.begin(),items.end(),[&](const auto&a,const auto&b){int order=0;
        if(column==2)order=a.length<b.length?-1:a.length>b.length?1:0;
        else if(column==3)order=a.uses<b.uses?-1:a.uses>b.uses?1:0;
        else{auto x=folded(column==1?wide(project.tracks[a.track].name):a.name),y=folded(column==1?wide(project.tracks[b.track].name):b.name);order=x<y?-1:x>y?1:0;}
        return descending?order>0:order<0;
    });
}
// Caller commits a candidate project only after the whole batch succeeds.
inline std::vector<std::pair<int,int>>placePoolItems(Project&candidate,const std::vector<PoolItem>&items,int target,double beat,bool layers){
    if(items.empty()||items.size()>TrackCount||target<0||target>=(int)candidate.tracks.size()||!std::isfinite(beat)||beat<0)throw std::runtime_error("Choose items and a Playlist track");
    if(layers&&target+(int)items.size()>TrackCount)throw std::runtime_error("Not enough free tracks");
    std::vector<std::pair<int,int>>placed;for(size_t i=0;i<items.size();i++){int t=layers?target+(int)i:target;while(t>=(int)candidate.tracks.size())appendTrack(candidate);int c=placePoolItem(candidate,items[i],t,beat);placed.push_back({t,c});if(!layers)beat+=candidate.tracks[t].clips[c].length;}
    return placed;
}
}
