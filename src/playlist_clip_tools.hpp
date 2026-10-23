#pragma once
#include "model.hpp"

namespace vamp {
enum class PlaylistClipEdit {None,Move,TrimStart,TrimEnd,StretchEnd,FadeIn,FadeOut};
// All edits are non-destructive. Source positions are seconds, timeline positions are beats.
inline void clampClipFades(Clip&c,const TempoMap&clock){double duration=clock.duration(c.beat,c.beat+c.length);c.fadeIn=clamp(c.fadeIn,0,duration);c.fadeOut=clamp(c.fadeOut,0,duration);}
inline Clip trimAudioStart(const Clip&source,double requested,const TempoMap&clock,double minimum=.01){
    Clip out=source;double end=source.beat+source.length,earliest=0;
    if(source.warp){double slope=source.warpPoints.size()>1?(source.warpPoints[1].second-source.warpPoints[0].second)/(source.warpPoints[1].beat-source.warpPoints[0].beat):60/source.warpBpm;earliest=std::max(0.,source.beat-source.offset/std::max(1e-9,slope));}
    else earliest=std::max(0.,clock.beat(clock.seconds(source.beat)-source.offset));
    out.beat=clamp(requested,earliest,end-std::min(minimum,source.length));out.length=end-out.beat;double delta=out.beat-source.beat;
    if(std::abs(delta)<1e-9)return out;
    if(source.warp){double second=delta>=0?warpSourceAt(source,delta):source.offset+delta*(source.warpPoints.size()>1?(source.warpPoints[1].second-source.warpPoints[0].second)/(source.warpPoints[1].beat-source.warpPoints[0].beat):60/source.warpBpm);out.offset=std::max(0.,second);out.warpPoints={{0,out.offset}};for(auto&p:source.warpPoints)if(p.beat>delta+1e-8&&p.beat<source.length-1e-8)out.warpPoints.push_back({p.beat-delta,p.second});out.warpPoints.push_back({out.length,warpSourceAt(source,source.length)});out.warpAudio.reset();out.warpKey=0;}
    else out.offset=std::max(0.,source.offset+clock.duration(source.beat,out.beat));
    // Keep the remaining portion of an existing fade, and keep the opposite edge fixed.
    out.fadeIn=std::max(0.,source.fadeIn-clock.duration(source.beat,out.beat));clampClipFades(out,clock);return out;
}
inline Clip trimAudioEnd(const Clip&source,double requested,const Sample&audio,const TempoMap&clock,double minimum=.01){
    Clip out=source;double latest=clock.beat(clock.seconds(source.beat)+std::max(0.,audio.left.size()/double(audio.rate)-source.offset));
    if(source.warp){double slope=source.warpPoints.size()>1?(source.warpPoints.back().second-source.warpPoints[source.warpPoints.size()-2].second)/(source.warpPoints.back().beat-source.warpPoints[source.warpPoints.size()-2].beat):60/source.warpBpm;latest=source.beat+source.length+std::max(0.,audio.left.size()/double(audio.rate)-warpSourceAt(source,source.length))/std::max(1e-9,slope);}
    out.length=clamp(requested-source.beat,std::min(minimum,source.length),std::max(std::min(minimum,source.length),std::min(16384.,latest)-source.beat));
    if(source.warp&&std::abs(out.length-source.length)>1e-9){double second=warpSourceAt(source,std::min(source.length,out.length));if(out.length>source.length){auto&a=source.warpPoints;double slope=a.size()>1?(a.back().second-a[a.size()-2].second)/(a.back().beat-a[a.size()-2].beat):60/source.warpBpm;second+=(out.length-source.length)*slope;}out.warpPoints={{0,source.offset}};for(auto&p:source.warpPoints)if(p.beat>1e-8&&p.beat<out.length-1e-8)out.warpPoints.push_back(p);out.warpPoints.push_back({out.length,second});out.warpAudio.reset();out.warpKey=0;}
    clampClipFades(out,clock);return out;
}
inline Clip stretchAudioEnd(const Clip&source,double requested,const Sample&audio,const TempoMap&clock,double minimum=.01){
    Clip out=source;out.warp=true;out.warpBpm=clock.bpmAt(source.beat);
    if(out.warpPoints.empty()){double end=std::min(audio.left.size()/double(audio.rate),source.offset+clock.duration(source.beat,source.beat+source.length));if(end<=source.offset+1./audio.rate)throw std::runtime_error("The clip contains no audio to stretch");out.warpPoints={{0,source.offset},{source.length,end}};}
    double sourceSeconds=out.warpPoints.back().second-out.warpPoints.front().second;
    double minEnd=clock.beat(clock.seconds(source.beat)+sourceSeconds*.25),maxEnd=clock.beat(clock.seconds(source.beat)+std::min(600.,sourceSeconds*4));
    out.length=clamp(requested,source.beat+std::min(minimum,source.length),16384)-source.beat;out.length=clamp(out.length,std::max(.001,minEnd-source.beat),std::min(16384-source.beat,maxEnd-source.beat));
    for(auto&p:out.warpPoints)p.beat*=out.length/source.length;
    double scale=clock.duration(out.beat,out.beat+out.length)/std::max(1e-9,clock.duration(source.beat,source.beat+source.length));out.fadeIn*=scale;out.fadeOut*=scale;out.warpAudio.reset();out.warpKey=0;clampClipFades(out,clock);return out;
}
}
