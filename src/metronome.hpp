#pragma once
#include "model.hpp"
namespace vamp {
inline float metronomeSample(int sound,double age,bool accent,uint32_t&noise){
    if(age<0||age>.075)return 0;constexpr double tau=6.283185307179586;
    double value=0,frequency=accent?1800:1100;
    switch(sound){
        case 1:value=(std::sin(tau*(accent?3200:2400)*age)*.8+std::sin(tau*(accent?6400:4800)*age)*.2)*std::exp(-age*260);break;
        case 2:frequency=accent?1050:780;value=(std::sin(tau*frequency*age)+.45*std::sin(tau*frequency*1.67*age))*std::exp(-age*115);break;
        case 3:noise^=noise<<13;noise^=noise>>17;noise^=noise<<5;value=((int32_t)noise/2147483648.*.72+std::sin(tau*(accent?1800:1250)*age)*.28)*std::exp(-age*210);break;
        case 4:frequency=accent?1320:880;value=(std::sin(tau*frequency*age)*.7+std::sin(tau*frequency*2.73*age)*.25+std::sin(tau*frequency*4.13*age)*.1)*std::exp(-age*65);break;
        default:value=std::sin(tau*frequency*age)*std::exp(-age*130);break;
    }
    return (float)(value*(accent?.19:.15));
}
}
