#pragma once
#include <array>
#include <complex>
#include <cmath>
#include <algorithm>
namespace vamp {
// Streaming phase vocoder. All storage is prepared before audio processing.
class NativePitch {
    static constexpr int N=2048,Hop=512,Bins=N/2+1;
    static constexpr double Pi=3.14159265358979323846;
    struct Channel {
        std::array<double,N>input{},output{},overlap{};
        std::array<double,Bins>lastPhase{},sumPhase{},magnitude{},frequency{},weights{};
        std::array<std::complex<double>,N>fft{};
    };
    std::array<Channel,2>channels{};std::array<double,N>window{};int cursor=N-Hop;
    static void transform(std::array<std::complex<double>,N>&a,bool inverse){
        for(int i=1,j=0;i<N;i++){int bit=N/2;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}
        for(int size=2;size<=N;size*=2){auto step=std::polar(1.,(inverse?2:-2)*Pi/size);for(int start=0;start<N;start+=size){std::complex<double>phase=1;for(int n=0;n<size/2;n++){auto even=a[start+n],odd=a[start+n+size/2]*phase;a[start+n]=even+odd;a[start+n+size/2]=even-odd;phase*=step;}}}
        if(inverse)for(auto&v:a)v/=N;
    }
    void frame(Channel&c,double ratio){
        for(int n=0;n<N;n++)c.fft[n]=c.input[n]*window[n];transform(c.fft,false);c.magnitude.fill(0);c.frequency.fill(0);c.weights.fill(0);
        for(int k=0;k<Bins;k++){
            double mag=std::abs(c.fft[k]),phase=std::arg(c.fft[k]);double expected=2*Pi*k*Hop/N;
            double residual=std::remainder(phase-c.lastPhase[k]-expected,2*Pi);c.lastPhase[k]=phase;
            double frequency=(k+residual*N/(2*Pi*Hop))*ratio;int target=(int)std::llround(k*ratio);
            if(target>=Bins)continue;c.magnitude[target]+=mag;c.frequency[target]+=frequency*mag;c.weights[target]+=mag;
        }
        c.fft.fill(0);
        for(int k=0;k<Bins;k++){
            double bin=c.weights[k]>1e-20?c.frequency[k]/c.weights[k]:k;c.sumPhase[k]=std::remainder(c.sumPhase[k]+2*Pi*bin*Hop/N,2*Pi);
            c.fft[k]=std::polar(c.magnitude[k],c.sumPhase[k]);if(k>0&&k<N/2)c.fft[N-k]=std::conj(c.fft[k]);
        }
        transform(c.fft,true);for(int n=0;n<N;n++)c.overlap[n]+=c.fft[n].real()*window[n]*(2./3.);
        std::copy_n(c.overlap.begin(),Hop,c.output.begin());std::move(c.overlap.begin()+Hop,c.overlap.end(),c.overlap.begin());std::fill(c.overlap.end()-Hop,c.overlap.end(),0);
        std::move(c.input.begin()+Hop,c.input.end(),c.input.begin());
    }
public:
    NativePitch(){for(int n=0;n<N;n++)window[n]=.5-.5*std::cos(2*Pi*n/N);}
    std::pair<double,double>sample(double l,double r,double leftRatio,double rightRatio){
        std::pair<double,double>result{channels[0].output[cursor-(N-Hop)],channels[1].output[cursor-(N-Hop)]};channels[0].input[cursor]=l;channels[1].input[cursor]=r;
        if(++cursor==N){cursor=N-Hop;frame(channels[0],leftRatio);frame(channels[1],rightRatio);}return result;
    }
};
}
