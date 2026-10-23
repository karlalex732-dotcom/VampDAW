#pragma once
#include "model.hpp"
#include "queue.hpp"
#include <thread>
namespace vamp {
// Single producer and consumer. Each endpoint keeps its own clock and FIFO.
class AsyncAudioBuffer {
    struct Frame{float l=0,r=0;};Spsc<Frame,65536>fifo;
    Frame a{},b{},c{},d{};double phase=0,ratio=1,integral=0;bool primed=false;int target=1536;
    static float cubic(float a,float b,float c,float d,double t){return (float)(b+.5*t*(c-a+t*(2*a-5*b+4*c-d+t*(3*(b-c)+d-a))));}
public:
    std::atomic<unsigned>overruns{0},underruns{0};std::atomic<double>correctionPpm{0};
    void reset(int latencyFrames=1536){fifo.reset();a=b=c=d={};phase=0;ratio=1;integral=0;primed=false;target=(int)clamp(latencyFrames,256,16384);overruns=0;underruns=0;correctionPpm=0;}
    void push(const float*interleaved,int frames){for(int i=0;i<frames;i++)if(!fifo.push({interleaved[i*2],interleaved[i*2+1]}))overruns++;}
    void push(const float*l,const float*r,int frames){for(int i=0;i<frames;i++)if(!fifo.push({l?l[i]:0,r?r[i]:0}))overruns++;}
    void read(float*out,int frames){if(!primed){if(fifo.size()<(size_t)target+4){std::fill_n(out,frames*2,0.f);return;}fifo.pop(a);fifo.pop(b);fifo.pop(c);fifo.pop(d);primed=true;}
        double error=(double(fifo.size())-target)/std::max(1,target);integral=clamp(integral+error*.0000001,-.003,.003);double desired=1+clamp(error*.002+integral,-.003,.003);ratio+=clamp(desired-ratio,-.00001,.00001);correctionPpm=(ratio-1)*1000000;
        for(int i=0;i<frames;i++){out[i*2]=cubic(a.l,b.l,c.l,d.l,phase);out[i*2+1]=cubic(a.r,b.r,c.r,d.r,phase);phase+=ratio;while(phase>=1){Frame next;if(!fifo.pop(next)){underruns++;primed=false;phase=0;std::fill(out+(i+1)*2,out+frames*2,0.f);return;}a=b;b=c;c=d;d=next;phase-=1;}}
    }
};
class AggregateOutput {
    std::thread worker;HANDLE stopEvent;void run(std::wstring id,int rate);
public:
    AsyncAudioBuffer buffer;std::atomic<int>state{0};std::atomic<double>latencyMs{0};
    AggregateOutput();~AggregateOutput();void start(const std::wstring&id,int rate);void stop();
};
}
