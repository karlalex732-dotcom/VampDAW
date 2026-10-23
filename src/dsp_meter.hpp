#pragma once
#include <algorithm>
#include <cmath>
namespace vamp {
struct DspMeter {
    double average=0,peak=0;bool initialized=false;
    void reset(){average=peak=0;initialized=false;}
    double update(double seconds,int frames,double rate){
        if(seconds<0||!std::isfinite(seconds)||frames<=0||rate<=0)return 0;
        double duration=frames/rate,load=seconds/duration*100;
        double blend=1-std::exp(-duration/.25);average=initialized?average+(load-average)*blend:load;initialized=true;
        peak=std::max(load,peak*std::exp(-duration));return load;
    }
};
inline unsigned sharedAudioPeriod(unsigned requested,unsigned normal,unsigned fundamental,unsigned minimum,unsigned maximum){
    if(maximum<minimum||!maximum)return normal;unsigned target=std::clamp(requested?requested:128u,minimum,maximum),step=std::max(1u,fundamental);
    auto rounded=(target+step-1)/step*step;if(rounded>maximum)rounded=maximum/step*step;return std::max(minimum,rounded);
}
}
