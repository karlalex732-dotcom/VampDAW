#include "note_tools.hpp"
#include <random>
#include <stdexcept>
namespace vamp {
bool scaleContains(int pitch,int root,int scale){static const std::vector<int>modes[]={{0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,3,5,7,8,11},{0,2,3,5,7,9,10},{0,2,4,6,7,9,11},{0,1,3,5,7,8,10},{0,2,4,5,7,9,10},{0,1,3,5,6,8,10},{0,2,3,5,7,9,11},{0,2,4,5,7,8,11},{0,2,4,7,9},{0,3,5,7,10},{0,3,5,6,7,10},{0,1,2,3,4,5,6,7,8,9,10,11}};if(scale<0)return true;int note=(pitch-root+1200)%12;for(int n:modes[(int)clamp(scale,0,ScaleCount-1)])if(n==note)return true;return false;}

std::vector<int>chordIntervals(int chord){switch(chord){case 0:return{0,4,7};case 1:return{0,3,7};case 2:return{0,4,7,11};case 3:return{0,3,7,10};case 4:return{0,4,7,10};case 5:return{0,2,7};case 6:return{0,5,7};case 7:return{0,3,6};case 8:return{0,4,8};case 9:return{0,7,12};case 10:return{0,4,7,14};default:return{0};}}
std::vector<Note>processNotes(const std::vector<Note>&source,const std::set<int>&selection,NoteTool tool,const NoteToolOptions&o,double length){
    std::vector<Note>chosen,others;for(size_t i=0;i<source.size();i++)(selection.empty()||selection.count((int)i)?chosen:others).push_back(source[i]);if(chosen.empty())return source;
    std::mt19937 random(o.seed);std::uniform_real_distribution<double>noise(-1,1);double grid=clamp(o.grid,1./128,16),first=length,last=0;int low=127,high=0;for(auto&n:chosen){first=std::min(first,n.beat);last=std::max(last,n.beat+n.length);low=std::min(low,n.pitch);high=std::max(high,n.pitch);}
    std::stable_sort(chosen.begin(),chosen.end(),[](auto&a,auto&b){return a.beat==b.beat?a.pitch<b.pitch:a.beat<b.beat;});
    std::vector<double>starts;if(tool==NoteTool::Legato){starts.reserve(source.size());for(auto&n:source)starts.push_back(n.beat);std::sort(starts.begin(),starts.end());}
    if(tool==NoteTool::Glue){std::vector<Note>merged;std::array<int,2048>lastIndex;lastIndex.fill(-1);for(auto n:chosen){int key=n.channel*128+n.pitch,index=lastIndex[key];if(index>=0&&merged[index].beat+merged[index].length>=n.beat-1e-8){auto&previous=merged[index];previous.length=std::max(previous.beat+previous.length,n.beat+n.length)-previous.beat;}else{lastIndex[key]=(int)merged.size();merged.push_back(n);}}chosen=std::move(merged);}
    else if(tool==NoteTool::Chop){std::vector<Note>pieces;for(auto n:chosen){double end=n.beat+n.length;for(double start=n.beat;start<end-1e-8;start+=grid){auto piece=n;piece.beat=start;piece.length=std::min(grid,end-start);pieces.push_back(piece);if(pieces.size()+others.size()>100000)throw std::runtime_error("Note tool exceeds 100000 notes");}}chosen=std::move(pieces);}
    else if(tool==NoteTool::Arpeggiate){std::vector<Note>arp;for(size_t i=0;i<chosen.size();){size_t end=i+1;double until=chosen[i].beat+chosen[i].length;while(end<chosen.size()&&std::abs(chosen[end].beat-chosen[i].beat)<1e-7){until=std::max(until,chosen[end].beat+chosen[end].length);end++;}int step=0;for(double beat=chosen[i].beat;beat<until-1e-7;beat+=grid){auto n=chosen[i+(step++%(end-i))];n.beat=beat;n.length=std::min(grid*.88,until-beat);arp.push_back(n);if(arp.size()+others.size()>100000)throw std::runtime_error("Arpeggiator exceeds 100000 notes");}i=end;}chosen=std::move(arp);}
    else{double groupBeat=-1;int strum=0;for(auto&n:chosen){switch(tool){
        case NoteTool::Quantize:{double index=std::round(n.beat/grid),target=index*grid+((int(index)&1)?grid*clamp(o.swing,0,.8):0);n.beat+=(target-n.beat)*clamp(o.strength,0,1);break;}
        case NoteTool::Humanize:n.beat+=noise(random)*grid*o.amount;n.velocity=(float)clamp(n.velocity+noise(random)*o.amount*.4,.01,1);break;
        case NoteTool::RandomVelocity:n.velocity=(float)clamp(n.velocity+noise(random)*o.amount,.01,1);break;
        case NoteTool::Legato:{auto next=std::upper_bound(starts.begin(),starts.end(),n.beat+1e-7);n.length=(next==starts.end()?length:*next)-n.beat;break;}
        case NoteTool::FlipTime:n.beat=first+last-n.beat-n.length;break;
        case NoteTool::FlipPitch:n.pitch=low+high-n.pitch;break;
        case NoteTool::Strum:if(std::abs(n.beat-groupBeat)>1e-7){groupBeat=n.beat;strum=0;}n.beat+=strum++*grid*o.amount;break;
        case NoteTool::FitScale:if(!scaleContains(n.pitch,o.root,o.scale)){for(int d=1;d<12;d++)if(scaleContains(n.pitch-d,o.root,o.scale)){n.pitch-=d;break;}else if(scaleContains(n.pitch+d,o.root,o.scale)){n.pitch+=d;break;}}break;
        case NoteTool::Transpose:n.pitch+=o.transpose;break;default:break;
    }}}
    for(auto&n:chosen){n.pitch=(int)clamp(n.pitch,0,127);n.beat=clamp(n.beat,0,std::max(0.,length-1./128));n.length=clamp(n.length,1./128,std::max(1./128,length-n.beat));}others.insert(others.end(),chosen.begin(),chosen.end());return others;
}
}
