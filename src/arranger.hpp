#pragma once
#include "model.hpp"
#include <set>
#include <random>
#include <cwctype>
#include <stdexcept>
namespace vamp {
enum class ArrangeRole{Auto,Drums,Bass,Harmony,Lead,Vocals,Texture,Fx,Ignore};
inline const wchar_t*arrangeRoleName(ArrangeRole r){const wchar_t*names[]={L"Auto",L"Drums",L"Bass",L"Harmony",L"Lead",L"Vocals",L"Texture",L"FX",L"Ignore"};return names[(int)r];}
struct ArrangeOptions{int style=0,bars=64,density=75,variation=60;uint32_t seed=1;bool append=false,protectVocals=true,fills=true;std::vector<ArrangeRole>roles;};
struct ArrangeSection{std::wstring name;int bars=8,energy=70;};
struct ArrangePlacement{int track;Clip clip;};
struct ArrangePlan{std::vector<ArrangeSection>sections;std::vector<ArrangeRole>roles;std::vector<ArrangePlacement>clips;std::vector<int>editableTracks;std::vector<std::wstring>warnings;double start=0,length=0;uint32_t seed=0;bool append=true;};
inline ArrangeRole analyzeArrangeRole(const Track&t){auto name=wide(t.name);for(auto&c:name)c=std::towlower(c);auto has=[&](std::initializer_list<const wchar_t*>words){for(auto w:words)if(name.find(w)!=std::wstring::npos)return true;return false;};
    if(has({L"vocal",L"voice",L"вокал",L"голос",L"acapella"}))return ArrangeRole::Vocals;
    if(t.midiChannel==10||has({L"drum",L"kick",L"snare",L"hat",L"clap",L"perc",L"удар",L"барабан"}))return ArrangeRole::Drums;
    if(has({L"bass",L"sub",L"бас"}))return ArrangeRole::Bass;
    if(has({L"lead",L"лид",L"solo",L"соло"}))return ArrangeRole::Lead;
    if(has({L"pad",L"chord",L"piano",L"keys",L"guitar",L"аккорд",L"клав",L"гитар"}))return ArrangeRole::Harmony;
    if(has({L"riser",L"impact",L"sweep",L"fx",L"эффект"}))return ArrangeRole::Fx;
    if(has({L"atmos",L"texture",L"ambien",L"атмос"}))return ArrangeRole::Texture;
    double pitch=0,duration=0;int count=0;auto notes=[&](const auto&ns){for(auto&n:ns)if(!n.muted){pitch+=n.pitch;duration+=n.length;count++;}};notes(t.notes);for(auto&p:t.patterns)notes(p.notes);
    if(count){if(pitch/count<49)return ArrangeRole::Bass;if(duration/count>1.1)return ArrangeRole::Harmony;return ArrangeRole::Lead;}
    return t.sample?ArrangeRole::Texture:ArrangeRole::Ignore;
}
struct ArrangeStyle{
    const wchar_t*name;std::array<const wchar_t*,8>sections;
    std::array<int,8>weights,energies;int drums=35,bass=45,lead=65,fx=55;bool bassInIntro=false;
};
inline const std::array<ArrangeStyle,20>&arrangeStyles(){
    static const std::array<ArrangeStyle,20>styles={{
        {L"Electronic",{L"Intro",L"Build",L"Drop",L"Break",L"Build II",L"Drop II",L"Bridge",L"Outro"},{1,2,3,1,2,3,2,1},{25,55,100,25,65,100,65,20}},
        {L"Pop",{L"Intro",L"Verse",L"Chorus",L"Verse II",L"Chorus II",L"Bridge",L"Final chorus",L"Outro"},{1,3,2,3,2,1,3,1},{25,55,90,60,95,45,100,20},30,40,60},
        {L"Hip-hop",{L"Intro",L"Verse",L"Hook",L"Verse II",L"Hook II",L"Break",L"Final hook",L"Outro"},{1,4,2,4,2,1,2,1},{25,65,90,70,95,35,100,20},30,40,70},
        {L"Rock",{L"Intro",L"Verse",L"Chorus",L"Verse II",L"Chorus II",L"Solo",L"Final chorus",L"Outro"},{1,3,2,3,2,2,3,1},{30,60,95,65,100,85,100,20},30,35,60,65,true},
        {L"Ambient",{L"Arrival",L"Motion",L"Bloom",L"Stillness",L"Rise",L"Horizon",L"Return",L"Dissolve"},{2,2,3,2,2,3,2,2},{20,40,65,25,55,85,50,15},80,60,55,35},
        {L"Dubstep",{L"Intro",L"Tension",L"Drop",L"Breakdown",L"Build II",L"Drop II",L"Half-time",L"Outro"},{1,2,4,2,2,4,1,1},{20,65,100,20,75,100,85,15},40,70,75,50},
        {L"Riddim",{L"Intro",L"Build",L"Call",L"Response",L"Reset",L"Double drop",L"Variation",L"Outro"},{1,1,3,3,1,4,2,1},{20,60,100,90,25,100,95,15},45,75,90,55},
        {L"House",{L"DJ intro",L"Groove",L"Hook",L"Breakdown",L"Build",L"Main drop",L"Groove II",L"DJ outro"},{3,3,2,2,1,4,2,3},{40,65,90,25,65,100,80,35},35,50,80,60},
        {L"Techno",{L"DJ intro",L"Pulse",L"Layers",L"Tension",L"Breakdown",L"Peak",L"Release",L"DJ outro"},{3,3,3,2,2,4,3,3},{40,60,75,85,20,100,75,35},35,50,85,60},
        {L"Drum & Bass",{L"Intro",L"Build",L"Drop",L"Rollout",L"Breakdown",L"Build II",L"Drop II",L"Outro"},{2,1,4,3,2,1,4,2},{25,65,100,85,20,75,100,30},40,70,75,55},
        {L"Trap",{L"Intro",L"Hook",L"Verse",L"Switch-up",L"Hook II",L"Verse II",L"Final hook",L"Outro"},{1,2,4,1,2,4,2,1},{20,95,65,45,100,70,100,20},30,55,80,50},
        {L"Phonk",{L"Intro",L"Drift",L"Hook",L"Break",L"Build",L"Drop",L"Drift II",L"Outro"},{1,3,3,1,1,4,3,1},{25,75,95,30,65,100,85,20},35,60,70,50},
        {L"Trance",{L"DJ intro",L"Drive",L"Theme",L"Breakdown",L"Lift",L"Euphoric drop",L"Reprise",L"DJ outro"},{2,3,2,4,2,4,2,2},{35,70,85,20,65,100,90,30},35,55,80,50},
        {L"Synthwave",{L"Intro",L"Night drive",L"Theme",L"Verse",L"Chorus",L"Solo",L"Final chorus",L"Outro"},{2,3,2,3,3,2,3,2},{25,65,85,55,95,80,100,20},35,45,75,65},
        {L"Reggaeton",{L"Intro",L"Hook",L"Verse",L"Pre-chorus",L"Chorus",L"Verse II",L"Final chorus",L"Outro"},{1,2,3,1,3,3,3,1},{25,90,65,50,100,70,100,25},30,40,80,60},
        {L"R&B",{L"Intro",L"Verse",L"Pre-chorus",L"Chorus",L"Verse II",L"Bridge",L"Final chorus",L"Outro"},{2,3,1,3,3,2,3,2},{20,50,65,85,55,40,95,15},30,40,75,65},
        {L"Lo-fi",{L"Intro",L"Loop A",L"Theme",L"Interlude",L"Loop B",L"Theme II",L"Wind-down",L"Outro"},{2,4,3,2,4,3,2,2},{20,50,65,25,55,70,40,15},40,45,60,60},
        {L"Future bass",{L"Intro",L"Verse",L"Build",L"Drop",L"Breakdown",L"Build II",L"Final drop",L"Outro"},{1,3,2,4,2,2,4,1},{20,50,65,100,25,75,100,20},40,70,75,50},
        {L"Metal",{L"Intro",L"Riff",L"Verse",L"Chorus",L"Breakdown",L"Solo",L"Final chorus",L"Outro"},{1,3,3,2,3,2,3,1},{45,85,70,100,90,95,100,35},30,35,70,65,true},
        {L"Cinematic",{L"Prologue",L"Discovery",L"Journey",L"Suspense",L"Rise",L"Climax",L"Aftermath",L"Epilogue"},{2,3,3,2,3,4,2,2},{15,35,55,40,70,100,45,10},65,60,70,35}
    }};return styles;
}
inline const ArrangeStyle&arrangeStyle(int style){return arrangeStyles()[std::clamp(style,0,(int)arrangeStyles().size()-1)];}
inline std::vector<ArrangeSection>defaultArrangeSections(const ArrangeOptions&o){
    auto&style=arrangeStyle(o.style);int total=std::clamp(o.bars,32,512),units=(total-32)/4,weight=0,used=0;for(int n:style.weights)weight+=n;
    std::array<int,8>counts{},remainders{};for(int i=0;i<8;i++){counts[i]=units*style.weights[i]/weight;used+=counts[i];remainders[i]=units*style.weights[i]%weight;}
    // Allocate whole four-bar blocks with stable ties, keeping the requested song length exact.
    for(int n=used;n<units;n++){int i=(int)(std::max_element(remainders.begin(),remainders.end())-remainders.begin());counts[i]++;remainders[i]=-1;}
    std::vector<ArrangeSection>out;for(int i=0;i<8;i++)out.push_back({style.sections[i],4+4*counts[i],style.energies[i]});out.back().bars+=total%4;return out;
}
inline ArrangePlan buildArrangement(const Project&p,const ArrangeOptions&o,const std::vector<ArrangeSection>&custom={}){
    ArrangePlan result;result.sections=custom.empty()?defaultArrangeSections(o):custom;result.append=o.append;result.seed=o.seed;double end=0;for(auto&t:p.tracks)for(auto&c:t.clips)end=std::max(end,c.beat+c.length);result.start=o.append?std::ceil(end/4)*4:0;
    for(auto&s:result.sections){if(s.bars<1||s.bars>512||s.energy<0||s.energy>100)throw std::runtime_error("Invalid arrangement section");result.length+=s.bars*4.;}if(result.sections.empty()||result.sections.size()>32||result.start+result.length>16384)throw std::runtime_error("Arrangement exceeds the song range");
    std::mt19937 random(o.seed);int skipped=0;bool automation=false;
    for(int id=0;id<(int)p.tracks.size();id++){auto&t=p.tracks[id];auto role=id<(int)o.roles.size()&&o.roles[id]!=ArrangeRole::Auto?o.roles[id]:analyzeArrangeRole(t);result.roles.push_back(role);
        if(t.frozen||t.external){skipped++;continue;}if(role==ArrangeRole::Ignore)continue;
        std::vector<Clip>palette;std::set<int>patterns;for(auto&c:t.clips)if(!c.muted&&c.length>0){if(clipAudio(t,c)||patterns.insert(c.pattern).second)palette.push_back(c);}
        if(palette.empty())for(int pattern=0;pattern<=(int)t.patterns.size();pattern++)if(!patternNotes(t,pattern).empty())palette.push_back(Clip{0,patternLength(t,pattern),pattern});
        if(palette.empty())continue;result.editableTracks.push_back(id);automation|=t.automationEnabled||!t.automationLanes.empty();
        if(role==ArrangeRole::Vocals&&o.protectVocals){double first=0;if(!t.clips.empty()){first=t.clips.front().beat;for(auto&c:t.clips)first=std::min(first,c.beat);}double cursor=0;auto takes=t.clips.empty()?palette:t.clips;
            for(auto c:takes){if(c.muted)continue;double at=t.clips.empty()?cursor:c.beat-first;if(at>=result.length)continue;c.beat=result.start+at;c.length=std::min(c.length,result.length-at);if(c.length>0)result.clips.push_back({id,c});cursor+=c.length;}continue;}
        auto&style=arrangeStyle(o.style);double at=result.start;for(size_t s=0;s<result.sections.size();s++){auto&section=result.sections[s];double stop=at+section.bars*4.;int threshold=role==ArrangeRole::Drums?style.drums:role==ArrangeRole::Bass?style.bass:role==ArrangeRole::Lead||role==ArrangeRole::Vocals?style.lead:role==ArrangeRole::Fx?style.fx:0;
            bool on=section.energy>=threshold;if(s==0&&(role==ArrangeRole::Lead||(role==ArrangeRole::Bass&&!style.bassInIntro)))on=false;
            if(on){double cursor=role==ArrangeRole::Fx?std::max(at,stop-4):at;size_t choice=(size_t)(section.energy*palette.size()/101);choice=std::min(choice,palette.size()-1);
                while(cursor<stop-1e-6){if(result.clips.size()>=50000)throw std::runtime_error("Arrangement has too many clips");bool last=cursor+4>=stop;size_t variant=choice;if((int)(random()%100)<std::clamp(o.variation,0,100)||(last&&o.fills&&role==ArrangeRole::Drums))variant=random()%palette.size();auto c=palette[variant];double length=std::min(c.length,stop-cursor);if(length<.0625)break;
                    bool keep=role==ArrangeRole::Drums||role==ArrangeRole::Bass||(int)(random()%100)<std::clamp(o.density,10,100);if(keep){c.beat=cursor;c.length=length;result.clips.push_back({id,c});}cursor+=length;
                }}at=stop;
        }
    }
    if(skipped)result.warnings.push_back(L"Frozen tracks and external MIDI are skipped");if(automation)result.warnings.push_back(L"Automation stays at its original song positions");if(result.clips.empty())throw std::runtime_error("Add patterns or audio, then assign track roles");return result;
}
inline void applyArrangement(Project&p,const ArrangePlan&plan){for(int id:plan.editableTracks)if(id<0||id>=(int)p.tracks.size()||p.tracks[id].frozen||p.tracks[id].external)throw std::runtime_error("Arrangement source has changed");for(auto&item:plan.clips)if(item.track<0||item.track>=(int)p.tracks.size()||!std::isfinite(item.clip.beat)||!std::isfinite(item.clip.length)||item.clip.beat<0||item.clip.length<=0||item.clip.beat+item.clip.length>16384)throw std::runtime_error("Invalid arrangement clip");
    if(!plan.append){for(int id:plan.editableTracks)p.tracks[id].clips.clear();p.loopStart=p.loopEnd=0;}
    // A preview defines the entire section map, including markers from older releases.
    p.markers.clear();for(auto&item:plan.clips)p.tracks[item.track].clips.push_back(item.clip);double beat=plan.start;int color=0;for(auto&s:plan.sections){p.markers.push_back({beat,utf8(s.name),color++%5});beat+=s.bars*4;}
    double end=plan.start+plan.length;for(auto&t:p.tracks)for(auto&c:t.clips)end=std::max(end,c.beat+c.length);p.bars=plan.append?std::max(p.bars,barsForEnd(end)):barsForEnd(end);
}
}
