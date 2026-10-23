#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "json/json.hpp"
#include "tempo_map.hpp"

namespace vamp {
using Json = nlohmann::json;
namespace fs = std::filesystem;
constexpr int TrackCount=512, MasterChannel=512, PluginSlotCount=16, MaxBlock=4096, MaxVoices=4096, MaxBlockEvents=8192;
constexpr double MinTempo=10.,MaxTempo=999.;
inline double clamp(double x, double lo, double hi) { return std::max(lo, std::min(hi, x)); }
inline int barsForEnd(double beat){return (int)clamp(std::ceil(beat/4-1e-5),1,4096);}
inline std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], n); return out;
}
inline std::string utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, 0); WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], n, nullptr, nullptr); return out;
}
struct Note { double beat=0, length=.5; int pitch=60; float velocity=.75f;float pan=0,fine=0;int channel=0;bool muted=false; };
struct Sample;
struct WarpPoint {double beat=0,second=0;};
struct Clip { double beat=0, length=4;int pattern=0;double offset=0;float gain=1;double fadeIn=0,fadeOut=0,patternOffset=0;std::string name;int color=-1;bool muted=false;std::shared_ptr<Sample> audio;bool warp=false;double warpBpm=120;std::vector<WarpPoint>warpPoints;std::shared_ptr<Sample>warpAudio;uint64_t warpKey=0;std::shared_ptr<Sample>warpReference;double warpReferenceOffset=0;std::string araId; };
struct Point { double beat=0; float value=1; };
struct AutomationLane {int target=0,slot=-1;uint32_t parameter=0;std::string name="Volume";bool enabled=true;std::vector<Point>points;};
struct Sample { bool missing=false;std::string loadError;int rate=48000; std::vector<float> left, right; fs::path path;std::shared_ptr<const uint8_t>identity=std::make_shared<const uint8_t>(0); };
struct Pattern {std::string name="Pattern";double length=4;std::vector<Note> notes;};
struct Send{int target=MasterChannel;float gain=.25f;bool sidechain=false,preFader=false;};
struct Slice{double start=0,end=0;float semitones=0,gain=1;bool reverse=false;};
struct Marker{double beat=0;std::string name="Section";int color=0;};
struct Track {
    int folder=-1;float playlistHeight=0; // 0 follows the workspace; otherwise logical pixels.
    bool frozen=false;double freezeBpm=0;std::shared_ptr<Sample>freezeAudio;
    std::string name="Empty",patternName="Pattern 1"; int color=0, sound=0;
    float volume=.65f, pan=0; bool mute=false, solo=false, external=false, automationEnabled=false;
    int midiChannel=1,inputPair=0; double patternLength=4;
    int automationTarget=0,automationSlot=-1;uint32_t automationParam=0;std::string automationName="Volume";
    std::vector<Note> notes; std::vector<Clip> clips; std::vector<Point> automation;
    std::vector<AutomationLane>automationLanes;
    std::shared_ptr<Sample> sample;
    bool sampler=false,chromatic=false,oneShot=true;int rootNote=36;std::vector<Slice>slices;double sampleBpm=0;std::string sampleKey;
    std::vector<std::string> pluginPaths;
    std::vector<Json> pluginStates;
    std::vector<bool>pluginBypass;std::vector<Pattern>patterns;int activePattern=0;
    int output=MasterChannel;std::vector<Send>sends;bool monitor=false,armed=false;
    bool audioInput=false;int inputMode=0,asioInputPair=0;std::string inputDevice;
};
inline std::shared_ptr<Sample> clipAudio(const Track&t,const Clip&c){return c.audio?c.audio:!t.sampler&&!t.audioInput?t.sample:nullptr;}
inline std::pair<const float*,const float*> inputChannels(int mode,const float*l,const float*r){if(!r)r=l;return mode==1?std::make_pair(l,l):mode==2?std::make_pair(r,r):std::make_pair(l,r);}
struct TrackFolder{int id=0,parent=-1;std::string name="Folder";bool collapsed=false;float playlistHeight=0;};
struct GrooveHit {double beat=0,offset=0;float velocity=1;};
struct Groove {std::string name="Groove";double length=4,grid=.25;std::vector<GrooveHit>hits;};
struct Project;
struct ProjectVersion {std::string name;std::shared_ptr<Project>snapshot;};
struct Project {
    std::vector<TempoPoint>tempoMap;std::vector<MeterPoint>meterMap;
    std::vector<Groove>grooves;std::vector<ProjectVersion>versions;std::shared_ptr<Project>abWorking;
    std::vector<TrackFolder>folders;
    std::string name="Nocturne"; double bpm=112; int bars=8; bool loop=true;
    double loopStart=0,loopEnd=0;std::vector<Marker>markers;
    std::vector<Track>tracks=std::vector<Track>(8);Track master;
    Project(){master.name="Master";master.volume=.8f;}
};
inline TempoMap songClock(const Project&p){return TempoMap(p.bpm,p.tempoMap,p.meterMap);}
inline double audioBeatLength(const Project&p,double start,double seconds){auto clock=songClock(p);return clock.beat(clock.seconds(start)+seconds)-start;}
inline double warpSourceAt(const Clip&c,double beat){if(c.warpPoints.empty())return c.offset+beat*60/c.warpBpm;auto i=std::upper_bound(c.warpPoints.begin(),c.warpPoints.end(),beat,[](double b,auto&p){return b<p.beat;});if(i==c.warpPoints.begin())return i->second;if(i==c.warpPoints.end())return c.warpPoints.back().second;auto a=i-1;return a->second+(i->second-a->second)*(beat-a->beat)/(i->beat-a->beat);}
inline void stripWarpCaches(Project&p){auto clear=[](Track&t){for(auto&c:t.clips){c.warpAudio.reset();c.warpKey=0;}};for(auto&t:p.tracks)clear(t);clear(p.master);}
inline void resizeWarp(Clip&c,double oldLength){if(c.warp&&oldLength>0){for(auto&p:c.warpPoints)p.beat*=c.length/oldLength;c.warpAudio.reset();c.warpKey=0;}}
inline void cutWarp(const Clip&original,Clip&left,Clip&right,double delta){double source=warpSourceAt(original,delta);left.warpPoints.clear();right.warpPoints={{0,source}};for(auto&p:original.warpPoints){if(p.beat<delta-1e-8)left.warpPoints.push_back(p);if(p.beat>delta+1e-8)right.warpPoints.push_back({p.beat-delta,p.second});}if(left.warpPoints.empty())left.warpPoints.push_back({0,original.offset});left.warpPoints.push_back({delta,source});if(right.warpPoints.size()==1)right.warpPoints.push_back({right.length,warpSourceAt(original,original.length)});right.offset=source;left.warpAudio.reset();right.warpAudio.reset();left.warpKey=right.warpKey=0;}
inline const std::vector<Note>&patternNotes(const Track&t,int id){return id>0&&id<=(int)t.patterns.size()?t.patterns[id-1].notes:t.notes;}
inline std::vector<Note>&patternNotes(Track&t,int id){return id>0&&id<=(int)t.patterns.size()?t.patterns[id-1].notes:t.notes;}
inline double patternLength(const Track&t,int id){return id>0&&id<=(int)t.patterns.size()?t.patterns[id-1].length:t.patternLength;}
inline void appendTrack(Project&p){if(p.tracks.size()>=TrackCount)return;Track t;t.name="Track "+std::to_string(p.tracks.size()+1);t.color=(int)p.tracks.size()%5;t.midiChannel=(int)p.tracks.size()%16+1;p.tracks.push_back(std::move(t));}
inline void removeTrack(Project&p,int index){if(index<0||index>=(int)p.tracks.size())return;p.tracks.erase(p.tracks.begin()+index);auto fix=[&](Track&t){if(t.output==index)t.output=MasterChannel;else if(t.output!=MasterChannel&&t.output>index)t.output--;t.sends.erase(std::remove_if(t.sends.begin(),t.sends.end(),[&](auto&s){return s.target==index;}),t.sends.end());for(auto&s:t.sends)if(s.target!=MasterChannel&&s.target>index)s.target--;};for(auto&t:p.tracks)fix(t);fix(p.master);if(p.tracks.empty())appendTrack(p);}
inline float automationAt(const std::vector<Point>& points, double beat) {
    if (points.empty()) return 1;
    if (beat <= points.front().beat) return points.front().value;
    auto next = std::upper_bound(points.begin(), points.end(), beat, [](double b, const Point& p){return b < p.beat;});
    if (next == points.end()) return points.back().value;
    auto prev = next-1; double d=next->beat-prev->beat;
    return (float)(prev->value + (next->value-prev->value)*(beat-prev->beat)/std::max(.000001,d));
}
template<class Emit>inline void visitAutomationBlock(const std::vector<Point>&points,double beat,double step,int frames,double loopLength,Emit emit,double loopStart=0){
    for(int first=0;first<frames;){double local=beat+first*step;if(loopLength>0){if(local>=loopLength)local=loopStart+std::fmod(local-loopLength,loopLength-loopStart);if(local<0)local=0;}int count=frames-first;
        if(loopLength>0&&step>0)count=std::min(count,std::max(1,(int)std::ceil((loopLength-local)/step-1e-8)));
        emit(first,automationAt(points,local));if(step>0){auto point=std::upper_bound(points.begin(),points.end(),local,[](double beat,const Point&p){return beat<p.beat;});for(;point!=points.end();point++){int offset=(int)std::llround((point->beat-local)/step);if(offset>=count-1)break;if(offset>0)emit(first+offset,point->value);}}
        if(count>1)emit(first+count-1,automationAt(points,local+(count-1)*step));first+=count;
    }
}
inline void putPoint(Track& t, double beat, float value, double tolerance=.08) {
    auto i=std::lower_bound(t.automation.begin(),t.automation.end(),beat,[](const Point&p,double b){return p.beat<b;});
    if(i!=t.automation.end() && std::abs(i->beat-beat)<tolerance) {i->beat=beat;i->value=value;}
    else if(i!=t.automation.begin() && std::abs((i-1)->beat-beat)<tolerance) {(i-1)->value=value;}
    else t.automation.insert(i,{beat,value});
    t.automationEnabled=true;
}
inline Project demoProject() {
    Project p;
    const char* names[]={"Velvet keys","Sub ritual","Heart / kick","Dust / snare","Silver hats","Midnight pad","Audio / vocal","External synth"};
    for(int t=0;t<(int)p.tracks.size();t++) {auto&tr=p.tracks[t];tr.name=names[t];tr.color=t%5;tr.sound=t;tr.midiChannel=t+1;}
    const int melody[]={60,67,63,70,67,63,58,62};
    for(int i=0;i<8;i++) p.tracks[0].notes.push_back({i*.5,.42,melody[i],.60f});
    for(int i=0;i<4;i++) p.tracks[1].notes.push_back({double(i),.75,i<2?36:34,.72f});
    for(int i=0;i<4;i++) p.tracks[2].notes.push_back({double(i),.15,36,.85f});
    p.tracks[3].notes={{1,.15,38,.68f},{3,.15,38,.75f}};
    for(int i=0;i<8;i++) p.tracks[4].notes.push_back({i*.5,.08,42,i%2?.28f:.40f});
    for(int pitch:{48,55,60,63}) p.tracks[5].notes.push_back({0,3.75,pitch,.26f});
    for(int t=0;t<6;t++) for(int bar=0;bar<8;bar++) if(t!=5 || bar%2==0) p.tracks[t].clips.push_back({bar*4.,4});
    p.tracks[0].automation={{0,.3f},{8,.85f},{12,.55f},{16,1},{24,.75f},{32,.3f}};
    p.tracks[0].automationEnabled=true;
    p.tracks[1].volume=.52f;p.tracks[2].volume=.6f;p.tracks[3].pan=-.08f;p.tracks[4].pan=.22f;
    p.tracks[7].external=true;
    return p;
}
std::shared_ptr<Sample> readWav(const fs::path& path);
Json projectJson(const Project& p, const fs::path& destination);
Project projectFromJson(const Json& j, const fs::path& source);
void saveProject(const Project& p, const fs::path& path);
Project loadProject(const fs::path& path);
}
