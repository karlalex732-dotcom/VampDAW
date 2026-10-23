#pragma once
#include "model.hpp"
#include <complex>

namespace vamp {
// The same transfer function drives audio and the equalizer response display.
struct NativeBiquad {
    double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    double run(double x){double y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}
    void set(int type,double hz,double q,double db,double rate){
        constexpr double pi=3.14159265358979323846;
        hz=clamp(hz,5,rate*.46);q=clamp(q,.1,20);
        double w=2*pi*hz/rate,c=std::cos(w),s=std::sin(w),a=s/(2*q),A=std::pow(10.,db/40.),a0=1+a;
        if(type==0){b0=1+a*A;b1=-2*c;b2=1-a*A;a0=1+a/A;a1=-2*c;a2=1-a/A;}
        else if(type==1){b0=(1-c)*.5;b1=1-c;b2=b0;a1=-2*c;a2=1-a;}
        else if(type==2){b0=(1+c)*.5;b1=-(1+c);b2=b0;a1=-2*c;a2=1-a;}
        else if(type==3){b0=a;b1=0;b2=-a;a1=-2*c;a2=1-a;}
        else if(type==4){double root=2*std::sqrt(A)*a;b0=A*((A+1)+(A-1)*c+root);b1=-2*A*((A-1)+(A+1)*c);b2=A*((A+1)+(A-1)*c-root);a0=(A+1)-(A-1)*c+root;a1=2*((A-1)-(A+1)*c);a2=(A+1)-(A-1)*c-root;}
        else if(type==5){double root=2*std::sqrt(A)*a;b0=A*((A+1)-(A-1)*c+root);b1=2*A*((A-1)-(A+1)*c);b2=A*((A+1)-(A-1)*c-root);a0=(A+1)+(A-1)*c+root;a1=-2*((A-1)+(A+1)*c);a2=(A+1)+(A-1)*c-root;}
        else if(type==6){b0=1;b1=-2*c;b2=1;a1=-2*c;a2=1-a;}
        else {b0=a0=1;b1=b2=a1=a2=0;}
        b0/=a0;b1/=a0;b2/=a0;a1/=a0;a2/=a0;
    }
    std::complex<double> response(double hz,double rate)const{
        auto z=std::polar(1.,-6.283185307179586*hz/rate);
        return(b0+b1*z+b2*z*z)/(1.+a1*z+a2*z*z);
    }
};
inline int eqBandBase(int band){return band<6?band*3:20;}
inline int eqFilterType(int mode){constexpr int types[]={0,5,4,2,1,3,6,7};return types[(int)clamp(mode,0,7)];}
}
