#pragma once
#include "model.hpp"
namespace vamp {
inline uint64_t inputChannelMask(int first,int mode=0){if(first<0||first>=64)return 0;uint64_t mask=0;if(mode!=2)mask|=1ull<<first;if(mode!=1&&first+1<64)mask|=1ull<<(first+1);return mask;}
struct AsioPort{std::wstring id,name;};std::vector<AsioPort>asioPorts();
float asioReadSample(const void*buffer,int sample,int format);void asioWriteSample(void*buffer,int sample,int format,float value);int asioSampleBytes(int format);
void asioReadBlock(const void*buffer,float*out,int frames,int format);void asioWriteBlock(void*buffer,const float*interleaved,int channel,int frames,int format);
class AsioStream{
public:
    struct Impl;std::unique_ptr<Impl>impl;
    using Callback=void(*)(void*,const float*,const float*,float*,int,const float*const*,int);
    using Timing=void(*)(void*,int,double);
    AsioStream();~AsioStream();
    void open(const std::wstring&id,double rate,int buffer,int inputPair,int outputPair,HWND window,Callback callback,void*context,Timing timing=nullptr,uint64_t inputMask=0);
    void start();void panel();bool restartNeeded()const;double rate()const;int frames()const;int inputLatency()const;int outputLatency()const;int inputs()const;int outputs()const;
};
}
