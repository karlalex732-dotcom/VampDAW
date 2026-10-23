#pragma once
#include "model.hpp"
#include "note_tools.hpp"
#include "drum_subgenres.hpp"
#include <random>
#include <set>
#include <map>
#include <stdexcept>

namespace vamp {
constexpr int DrumRoles=8;
inline const wchar_t* drumRoleName(int i){static const wchar_t*n[]={L"Kick",L"Snare / clap",L"Closed hat",L"Open hat",L"Percussion",L"Tom",L"Crash",L"Ride"};return n[std::clamp(i,0,7)];}
struct RhythmOptions{int style=5,bars=8,subgenre=0;double density=.7,swing=0,humanize=.15,variation=.35;bool fills=true;uint32_t seed=1;};
using DrumPart=std::array<std::vector<Note>,DrumRoles>;
inline RhythmOptions cleanRhythm(RhythmOptions o){o.style=std::clamp(o.style,0,DrumGenreCount-1);o.subgenre=std::clamp(o.subgenre,0,DrumSubgenreCount-1);o.bars=std::clamp(o.bars,1,64);o.density=clamp(o.density,0,1);o.swing=clamp(o.swing,0,.65);o.humanize=clamp(o.humanize,0,1);o.variation=clamp(o.variation,0,1);return o;}
inline DrumPart generateSubgenreRhythm(const RhythmOptions&o){
    auto&profile=drumSubgenres()[o.style][o.subgenre-1];DrumPart result;
    std::mt19937 rng(o.seed^uint32_t(o.style*2654435761u)^uint32_t(o.subgenre*2246822519u));
    auto random=[&]{return std::generate_canonical<double,24>(rng);};
    for(int bar=0;bar<o.bars;bar++)for(int step=0;step<16;step++){
        bool fill=o.fills&&(bar+1)%4==0&&step>=12;
        std::array<uint16_t,DrumRoles>masks{profile.kick,profile.snare,profile.hats,profile.open,profile.percussion,0,0,profile.ride};
        for(int role=0;role<DrumRoles;role++){
            bool hit=(masks[role]&(1u<<step))!=0;
            if(role==5)hit=fill&&(step==12||step==14||step==15);
            if(role==6)hit=step==0&&bar%4==0;
            if(role==0&&profile.roll==3&&fill&&step%2==0)hit=true;
            if((role==0||role==1)&&!hit&&step%2&&random()<o.variation*.12*o.density)hit=true;
            if(role==1&&fill&&step%2==0&&random()<o.variation*.7)hit=true;
            if(role>=2&&role!=6&&hit&&random()>o.density)hit=false;
            if(!hit)continue;
            double at=bar*4+step*.25+(step%2?o.swing*.25:0)+(random()-.5)*o.humanize*.035;
            at=clamp(at,0,o.bars*4-.02);
            float velocity=(float)clamp((role==0?.95:role==1?.88:role==6?.8:step%4==0?.76:.54)+(random()-.5)*o.humanize*.3,.16,1);
            result[role].push_back({at,.12,36,velocity});
            if(role==2&&profile.roll>0&&profile.roll<3&&bar%2==1&&step>=12&&o.density>.45){
                int extra=profile.roll==2?2:1;double increment=profile.roll==2?1./12:.125;
                for(int i=1;i<=extra;i++)if(random()<o.density*(.65+o.variation*.35)){
                    double next=std::min(o.bars*4-.02,at+i*increment);
                    result[role].push_back({next,.055,36,velocity*(float)(.72+.06*i)});
                }
            }
        }
    }
    for(auto&lane:result)std::stable_sort(lane.begin(),lane.end(),[](auto&a,auto&b){return a.beat<b.beat;});return result;
}
inline DrumPart generateRhythm(RhythmOptions options){
    auto o=cleanRhythm(options);if(o.subgenre)return generateSubgenreRhythm(o);DrumPart result;std::mt19937 rng(o.seed^uint32_t(o.style*2654435761u));auto random=[&]{return std::generate_canonical<double,24>(rng);};
    bool four=o.style==7||o.style==8||o.style==12,half=o.style==5||o.style==6||o.style==10||o.style==11||o.style==17;
    for(int bar=0;bar<o.bars;bar++)for(int step=0;step<16;step++){
        bool kick=step==0,snare=half?step==8:step==4||step==12;
        if(four)kick=step%4==0;else switch(o.style){
        case 0:case 1:case 13:kick=step==0||step==6||step==8;break;
        case 2:case 10:case 11:case 16:kick=step==0||step==7||step==10;break;
        case 3:case 18:kick=step==0||step==2||step==8||step==10;break;
        case 4:case 19:kick=step==0;snare=step==12;break;
        case 5:case 6:case 17:kick=step==0||step==6||step==11;break;
        case 9:kick=step==0||step==10;break;
        case 14:kick=step==0||step==8;snare=step==3||step==6||step==11||step==14;break;
        case 15:kick=step==0||step==7||step==10;break;
        default:kick=step==0||step==8;break;}
        bool fill=o.fills&&(bar+1)%4==0&&step>=12;
        for(int role=0;role<DrumRoles;role++){
            bool hit=role==0?kick:role==1?snare:role==2?(step%2==0&&step%4!=2?true:step%2==0||random()<o.density*.45):role==3?step%8==6:role==4?step%4==3&&random()<o.density*.65:role==5?fill&&step%2==1:role==6?bar%4==0&&step==0:step%4==2&&o.style==9;
            if((role==0||role==1)&&!hit&&step%2&&random()<o.variation*.11*o.density)hit=true;
            if(role==1&&fill&&step%2==0)hit=true;
            if(role>=2&&role!=6&&hit&&random()>o.density)hit=false;
            if(role==2&&o.density>.85&&(o.style==10||o.style==6)&&step>=12&&bar%2)hit=true;
            if(!hit)continue;
            double beat=bar*4+step*.25+(step%2?o.swing*.25:0)+(random()-.5)*o.humanize*.035;
            beat=clamp(beat,0,o.bars*4-.02);float velocity=(float)clamp((role==0?.95:role==1?.88:step%4==0?.74:.55)+(random()-.5)*o.humanize*.3,.18,1);
            result[role].push_back({beat,.12,36,velocity});
            if(role==2&&o.style==10&&fill&&o.density>.8&&step%2==0)result[role].push_back({std::min(o.bars*4-.02,beat+.125),.06,36,velocity*.7f});
        }
    }
    for(auto&lane:result)std::stable_sort(lane.begin(),lane.end(),[](auto&a,auto&b){return a.beat<b.beat;});return result;
}
inline const wchar_t* progressionName(int i){static const wchar_t*n[]={L"I – V – vi – IV",L"vi – IV – I – V",L"ii – V – I – I",L"I – vi – IV – V",L"i – VI – III – VII",L"i – VII – VI – VII",L"i – iv – VI – V",L"I – IV – V – IV",L"i – ii – v – i",L"I – iii – vi – IV",L"i – VI – iv – VII",L"I – IV – ii – V"};return n[std::clamp(i,0,11)];}
struct HarmonyOptions{int root=0,scale=1,bars=8,progression=4,voicing=0,octave=4;double chordBeats=4,strum=0;uint32_t seed=1;};
struct MelodyOptions{double density=.65,gate=.8,variation=.35;int rhythm=0;uint32_t seed=1;bool bass=false;};
inline std::vector<int> generatorScale(int root,int mode){std::vector<int>out;for(int pitch=0;pitch<128;pitch++)if(scaleContains(pitch,root,mode))out.push_back(pitch);return out;}
inline int progressionDegree(int preset,int chord){static const int p[][4]={{0,4,5,3},{5,3,0,4},{1,4,0,0},{0,5,3,4},{0,5,2,6},{0,6,5,6},{0,3,5,4},{0,3,4,3},{0,1,4,0},{0,2,5,3},{0,5,3,6},{0,3,1,4}};return p[std::clamp(preset,0,11)][chord%4];}
inline HarmonyOptions cleanHarmony(HarmonyOptions o){o.root=std::clamp(o.root,0,11);o.scale=std::clamp(o.scale,0,ScaleCount-1);o.bars=std::clamp(o.bars,1,64);o.progression=std::clamp(o.progression,0,11);o.voicing=std::clamp(o.voicing,0,3);o.octave=std::clamp(o.octave,2,7);o.chordBeats=clamp(o.chordBeats,1,16);o.strum=clamp(o.strum,0,.2);return o;}
inline std::vector<Note> generateChords(HarmonyOptions options){
    auto o=cleanHarmony(options);std::mt19937 rng(o.seed);auto scale=generatorScale(o.root,o.scale);auto start=std::lower_bound(scale.begin(),scale.end(),o.octave*12+o.root);int base=(int)(start-scale.begin());std::vector<Note>out;std::vector<int>previous;
    for(double beat=0;beat<o.bars*4;beat+=o.chordBeats){int degree=progressionDegree(o.progression,(int)std::round(beat/o.chordBeats)),voices=o.voicing==1?4:o.voicing==2?5:3;std::vector<int>pitches;
        for(int n=0;n<voices;n++)pitches.push_back(scale[std::clamp(base+degree+n*2,0,(int)scale.size()-1)]);
        if(previous.empty()){for(unsigned inv=0;inv<o.seed%voices;inv++){std::sort(pitches.begin(),pitches.end());pitches.front()+=12;}for(auto&pitch:pitches)while(pitch>108)pitch-=12;std::sort(pitches.begin(),pitches.end());}
        else{double best=1e9;auto candidate=pitches;for(int shift=-2;shift<=2;shift++){auto trial=pitches;for(auto&pitch:trial)pitch+=shift*12;for(int inversion=0;inversion<voices;inversion++){std::sort(trial.begin(),trial.end());double score=std::generate_canonical<double,24>(rng)*3;for(int n=0;n<voices;n++)score+=std::abs(trial[n]-previous[n]);if(trial.front()>=24&&trial.back()<=108&&score<best){best=score;candidate=trial;}trial[0]+=12;}}pitches=candidate;}
        previous=pitches;if(o.voicing==3&&pitches.size()>1)pitches[1]+=12;
        std::sort(pitches.begin(),pitches.end());for(int n=0;n<voices;n++){double at=beat+n*o.strum,end=std::min(beat+o.chordBeats,o.bars*4.);if(at<end)out.push_back({at,std::max(.01,end-at-.035),pitches[n],(float)(.72-n*.025)});}
    }return out;
}
inline std::vector<Note> generateBassLine(HarmonyOptions harmony,MelodyOptions o){
    auto h=cleanHarmony(harmony);auto scale=generatorScale(h.root,h.scale);int base=(int)(std::lower_bound(scale.begin(),scale.end(),h.octave*12+h.root)-scale.begin());std::mt19937 rng(o.seed);auto random=[&]{return std::generate_canonical<double,24>(rng);};double step=o.rhythm==1?.25:o.rhythm==2?1./3:o.rhythm==3?1:.5;std::vector<Note>out;int count=(int)std::round(h.bars*4/step);
    for(int i=0;i<count;i++){double beat=i*step;bool strong=std::abs(beat/h.chordBeats-std::round(beat/h.chordBeats))<1e-6;bool down=std::abs(beat-std::round(beat))<1e-6;if(!strong&&random()>clamp(o.density,0,1)*(down?1:.72))continue;int degree=progressionDegree(h.progression,(int)(beat/h.chordBeats)),tone=0;if(!strong&&random()<clamp(o.variation,0,1))tone=(rng()%3)*2;int index=std::clamp(base+degree+tone,0,(int)scale.size()-1);int pitch=scale[index];if(!strong&&random()<o.variation*.25)pitch+=12;while(pitch>72)pitch-=12;pitch=std::max(12,pitch);double length=std::min(step*clamp(o.gate,.1,1),h.bars*4-beat);out.push_back({beat,length,pitch,(float)(strong?.9:down?.8:.66+random()*.13)});
    }return out;
}
inline std::vector<Note> generateMelody(HarmonyOptions harmony,MelodyOptions o){
    if(o.bass)return generateBassLine(harmony,o);
    auto h=cleanHarmony(harmony);o.density=clamp(o.density,0,1);o.gate=clamp(o.gate,.1,1);o.variation=clamp(o.variation,0,1);o.rhythm=std::clamp(o.rhythm,0,3);auto scale=generatorScale(h.root,h.scale);auto rng=std::mt19937(o.seed);auto random=[&]{return std::generate_canonical<double,24>(rng);};
    int base=(int)(std::lower_bound(scale.begin(),scale.end(),h.octave*12+h.root)-scale.begin());std::vector<Note>out;double step=o.rhythm==1?.25:o.rhythm==2?1./3:o.rhythm==3?1:.5;int count=(int)std::round(h.bars*4/step),phrase=(int)std::round(8/step);std::vector<int>motif(phrase);for(auto&v:motif)v=(int)(rng()%9)-4;
    int position=0;for(int i=0;i<count;i++){double beat=i*step;bool strong=std::abs(beat-std::round(beat))<1e-6;if(i&&random()>o.density*(strong?1:.8))continue;int degree=progressionDegree(h.progression,(int)(beat/h.chordBeats));int move=motif[i%phrase];if(i>=phrase&&random()<o.variation)move+=(int)(rng()%3)-1;position=std::clamp((position+move)/2,-3,9);
        int index=base+position;if(strong){int chordTone=(int)(rng()%3)*2+degree;while(chordTone-position>4)chordTone-=7;while(position-chordTone>4)chordTone+=7;index=base+chordTone;}index=std::clamp(index,std::max(0,base-4),std::min((int)scale.size()-1,base+11));double length=std::min(step*o.gate,h.bars*4-beat);out.push_back({beat,length,scale[index],(float)(strong?.82:.62+random()*.14)});
    }return out;
}
struct GeneratorLane{int track=-1;std::shared_ptr<Sample>sample;};
struct GeneratedProject{Project project;std::vector<int>tracks;double start=0,length=0;};
// Transactional: validate and build a candidate before the UI changes the live song.
inline GeneratedProject makeDrumProject(const Project&source,const RhythmOptions&options,const std::array<GeneratorLane,DrumRoles>&lanes,bool place,double start,bool replace=false){
    GeneratedProject out{source,{},std::max(0.,start),cleanRhythm(options).bars*4.};auto part=generateRhythm(options);std::map<int,std::vector<Note>>notes;
    for(int r=0;r<DrumRoles;r++){auto lane=lanes[r];int track=lane.track;if(lane.sample){if(out.project.tracks.size()>=TrackCount)throw std::runtime_error("Track limit reached");track=(int)out.project.tracks.size();appendTrack(out.project);auto&t=out.project.tracks.back();t.name=utf8(drumRoleName(r))+" / "+utf8(lane.sample->path.stem().wstring());t.sample=lane.sample;t.sampler=true;t.chromatic=true;t.oneShot=true;t.rootNote=36;t.volume=.75f;}
        if(track<0)continue;if(track>=(int)out.project.tracks.size())throw std::runtime_error("Choose a valid sample track");auto&t=out.project.tracks[track];if(t.frozen||!t.sampler||!t.sample)throw std::runtime_error("Choose an unfrozen sample instrument");if(part[r].empty())continue;if(t.slices.empty())t.slices.push_back({0,t.sample->left.size()/double(t.sample->rate)});
        for(auto n:part[r]){n.pitch=t.rootNote;n.length=std::min(n.length,out.length-n.beat);notes[track].push_back(n);}
    }
    if(notes.empty())throw std::runtime_error("Load a sample or assign a sample instrument first");
    for(auto&[track,events]:notes){auto&t=out.project.tracks[track];std::stable_sort(events.begin(),events.end(),[](auto&a,auto&b){return a.beat<b.beat;});if(replace){patternNotes(t,t.activePattern)=events;if(t.activePattern)t.patterns[t.activePattern-1].length=out.length;else t.patternLength=out.length;}else{if(t.patterns.size()>=255)throw std::runtime_error("Pattern limit reached");t.patterns.push_back({"Blood Beat / "+(options.subgenre?utf8(drumSubgenreName(options.style,options.subgenre))+" / ":"")+std::to_string(options.seed),out.length,events});t.activePattern=(int)t.patterns.size();}if(place)t.clips.push_back({out.start,out.length,t.activePattern});out.tracks.push_back(track);}
    if(place)out.project.bars=std::max(out.project.bars,barsForEnd(out.start+out.length));return out;
}
inline GeneratedProject makeHarmonyProject(const Project&source,int track,const HarmonyOptions&h,const MelodyOptions&m,bool melody,bool place,double start,bool replace=false){
    if(track<0||track>=(int)source.tracks.size()||source.tracks[track].frozen)throw std::runtime_error("Choose an unfrozen instrument track");auto&t0=source.tracks[track];if(t0.audioInput||(t0.sample&&(!t0.sampler||!t0.chromatic)))throw std::runtime_error("Choose a MIDI instrument or chromatic sampler");
    GeneratedProject out{source,{track},std::max(0.,start),cleanHarmony(h).bars*4.};auto&t=out.project.tracks[track];auto notes=melody?generateMelody(h,m):generateChords(h);if(replace){patternNotes(t,t.activePattern)=notes;if(t.activePattern)t.patterns[t.activePattern-1].length=out.length;else t.patternLength=out.length;}else{if(t.patterns.size()>=255)throw std::runtime_error("Pattern limit reached");t.patterns.push_back({melody?(m.bass?"Night Bass":"Night Melody"):"Blood Chords",out.length,notes});t.activePattern=(int)t.patterns.size();}if(place){t.clips.push_back({out.start,out.length,t.activePattern});out.project.bars=std::max(out.project.bars,barsForEnd(out.start+out.length));}return out;
}
}
