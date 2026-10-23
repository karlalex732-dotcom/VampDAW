#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
namespace vamp {
struct TempoPoint {double beat=0,bpm=120;};
struct MeterPoint {double beat=0;int numerator=4,denominator=4;};
struct MusicalPosition {int bar=1,beat=1,tick=0;};
// Positions are quarter notes. Tempo changes are steps; meter changes start a new bar.
class TempoMap {
    struct Segment {double beat,seconds,bpm;};
    struct Meter {double beat;int numerator,denominator,bar;};
    std::vector<Segment> tempos;std::vector<Meter> meters;
public:
    TempoMap(double base=120,const std::vector<TempoPoint>&points={},const std::vector<MeterPoint>&signatures={}){
        if(!std::isfinite(base)||base<10||base>999||points.size()>2048||signatures.size()>1024)throw std::runtime_error("Invalid tempo map");
        tempos.push_back({0,0,base});auto sorted=points;std::stable_sort(sorted.begin(),sorted.end(),[](auto&a,auto&b){return a.beat<b.beat;});
        double previous=-1;for(auto&p:sorted){if(!std::isfinite(p.beat)||!std::isfinite(p.bpm)||p.beat<0||p.beat>16384||p.bpm<10||p.bpm>999||p.beat==previous)throw std::runtime_error("Invalid tempo point");previous=p.beat;auto last=tempos.back();double seconds=last.seconds+(p.beat-last.beat)*60/last.bpm;if(p.beat==0)tempos.back()={0,0,p.bpm};else tempos.push_back({p.beat,seconds,p.bpm});}
        meters.push_back({0,4,4,1});auto ms=signatures;std::stable_sort(ms.begin(),ms.end(),[](auto&a,auto&b){return a.beat<b.beat;});previous=-1;
        for(auto&p:ms){if(!std::isfinite(p.beat)||p.beat<0||p.beat>16384||p.beat==previous||p.numerator<1||p.numerator>32||p.denominator<1||p.denominator>32||(p.denominator&(p.denominator-1)))throw std::runtime_error("Invalid time signature");previous=p.beat;auto m=meters.back();int bar=m.bar+(int)std::ceil((p.beat-m.beat)/(m.numerator*4./m.denominator)-1e-9);if(p.beat==0)meters.back()={0,p.numerator,p.denominator,1};else meters.push_back({p.beat,p.numerator,p.denominator,bar});}
    }
    double seconds(double beat)const{auto i=std::upper_bound(tempos.begin(),tempos.end(),beat,[](double b,auto&s){return b<s.beat;});if(i!=tempos.begin())--i;return i->seconds+(beat-i->beat)*60/i->bpm;}
    double beat(double seconds)const{auto i=std::upper_bound(tempos.begin(),tempos.end(),seconds,[](double t,auto&s){return t<s.seconds;});if(i!=tempos.begin())--i;return i->beat+(seconds-i->seconds)*i->bpm/60;}
    double bpmAt(double beat)const{auto i=std::upper_bound(tempos.begin(),tempos.end(),beat+1e-9,[](double b,auto&s){return b<s.beat;});return (i==tempos.begin()?i:i-1)->bpm;}
    double nextChange(double beat)const{auto i=std::upper_bound(tempos.begin(),tempos.end(),beat+1e-8,[](double b,auto&s){return b<s.beat;});double next=i==tempos.end()?std::numeric_limits<double>::infinity():i->beat;auto m=std::upper_bound(meters.begin(),meters.end(),beat+1e-8,[](double b,auto&s){return b<s.beat;});return std::min(next,m==meters.end()?std::numeric_limits<double>::infinity():m->beat);}
    MeterPoint meterAt(double b)const{auto i=std::upper_bound(meters.begin(),meters.end(),b+1e-9,[](double b,auto&m){return b<m.beat;});auto&m=*(i==meters.begin()?i:i-1);return {m.beat,m.numerator,m.denominator};}
    MusicalPosition musical(double b)const{b=std::max(0.,b);auto i=std::upper_bound(meters.begin(),meters.end(),b+1e-9,[](double b,auto&m){return b<m.beat;});auto&m=*(i==meters.begin()?i:i-1);double pulse=4./m.denominator,local=(b-m.beat)/pulse;int whole=(int)std::floor(local+1e-8);int tick=(int)std::floor((local-whole)*960+1e-5);return {m.bar+whole/m.numerator,whole%m.numerator+1,std::max(0,std::min(959,tick))};}
    double barStart(double b)const{auto m=meterAt(b);double length=m.numerator*4./m.denominator;return m.beat+std::floor((b-m.beat)/length+1e-9)*length;}
    double duration(double a,double b)const{return seconds(b)-seconds(a);}
};
}
