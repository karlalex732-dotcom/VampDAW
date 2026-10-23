#pragma once
#include "playlist_clip_tools.hpp"
#include "song_tools.hpp"
static void playlistClipTests(){
    Project p;p.bpm=120;p.tempoMap={{0,120},{8,60}};auto clock=songClock(p);Clip c;c.beat=4;c.length=8;c.offset=1;c.fadeIn=.5;c.fadeOut=.7;Sample source;source.rate=48000;source.left.resize(480000);source.right=source.left;
    auto a=trimAudioStart(c,6,clock);check(a.beat==6&&a.length==6&&a.offset==2&&a.fadeIn==0&&a.fadeOut==.7,"Left trim keeps the endpoint, advances source time, and retains opposite fade");
    a=trimAudioStart(c,0,clock);check(a.beat==2&&a.offset==0&&a.beat+a.length==12,"Extending the left edge stops at source zero");
    a=trimAudioStart(c,10,clock);check(a.offset==5&&a.length==2,"Left trim follows tempo changes in seconds");
    auto b=trimAudioEnd(c,10,source,clock);check(b.beat==4&&b.length==6&&b.offset==1,"Right trim never stretches unwarped audio");
    c.warp=true;c.offset=2;c.warpPoints={{0,2},{4,4},{8,6}};a=trimAudioStart(c,6,clock);check(a.warp&&a.offset==3&&a.warpPoints.front().second==3&&a.warpPoints.back().beat==6&&a.warpPoints.back().second==6,"Warp left trim retains source anchors instead of rescaling them");
    a=trimAudioStart(c,0,clock);check(a.offset==0&&a.warpPoints.front().second==0&&a.warpPoints.back().second==6,"Warp left extension extrapolates only as far as source zero");
    p.bpm=120;p.tempoMap.clear();clock=songClock(p);c={};c.length=4;c.fadeIn=.1;c.fadeOut=.2;source.left.resize(96000);source.right.resize(96000);for(size_t i=0;i<source.left.size();i++)source.left[i]=source.right[i]=(float)(.2*std::sin(i*6.283185307179586*440/48000));
    a=stretchAudioEnd(c,8,source,clock);check(a.warp&&a.length==8&&a.warpPoints.back().second==2&&a.fadeIn==.2&&a.fadeOut==.4,"Stretch scales musical anchors and fade durations without changing the source");auto rendered=renderWarp(source,a,clock);check(rendered->left.size()==192000,"Stretch produces the requested timeline duration");int crossings=0;for(size_t i=48001;i<144000;i++)if(rendered->left[i-1]<=0&&rendered->left[i]>0)crossings++;check(std::abs(crossings/2.-440)<4,"Playlist stretch preserves the source pitch");
    auto trimmed=trimAudioStart(a,2,clock);auto warped=renderWarp(source,trimmed,clock);check(warped->left.size()==144000&&trimmed.warpPoints.front().second==.5,"Trimming an already stretched clip preserves its source-time mapping");
    Json report={{"passed",true},{"tempo_aware_trim",true},{"non_destructive_extension",true},{"warp_trim",true},{"stretch_frames",rendered->left.size()},{"measured_pitch_hz",crossings/2.},{"scaled_fades",true}};std::ofstream(L"work/test-results/playlist-clips.json")<<report.dump(2);
}
