#include "native_fx.hpp"
#include "native_pitch.hpp"
#include "native_biquad.hpp"
#include <stdexcept>
#include <numeric>
namespace vamp {
namespace {
constexpr double pi=3.14159265358979323846;
double amp(double db){return std::pow(10.,db/20.);}
double level(double x){return 20*std::log10(std::max(1e-12,x));}
double pole(double ms,double rate){return std::exp(-1./(std::max(.01,ms)*.001*rate));}
using Biquad=NativeBiquad;
struct Delay {
    std::vector<double>l,r;size_t head=0;
    explicit Delay(size_t size):l(size,0),r(size,0){}
    double read(int channel,double frames)const{auto&v=channel?r:l;double at=double(head)-clamp(frames,1,double(v.size()-2));while(at<0)at+=v.size();double integral=std::floor(at);size_t a=(size_t)integral%v.size(),b=(a+1)%v.size();return v[a]+(v[b]-v[a])*(at-integral);}
    void write(double a,double b){l[head]=a;r[head]=b;head=(head+1)%l.size();}
};
struct ReverbLine {
    std::vector<double>v;size_t at=0;double low=0,base=1,delay=1;
    explicit ReverbLine(size_t n):v((size_t)(n*1.6)+4,0),base(n),delay(n){}
    double read(){double pos=double(at)-clamp(delay,1,double(v.size()-2));while(pos<0)pos+=v.size();double integral=std::floor(pos);size_t a=(size_t)integral%v.size(),b=(a+1)%v.size();return v[a]+(v[b]-v[a])*(pos-integral);}
    void write(double x){v[at]=x;if(++at==v.size())at=0;}
    double comb(double x,double feedback,double damping){double y=read();low+=(y-low)*damping;write(x+low*feedback);return y;}
    double allpass(double x){double z=read(),y=z-.5*x;write(x+.5*y);return y;}
};
struct Loudness {
    Biquad k[2][2];std::array<double,4>energy{};int slot=0,parts=0,frames=0,blockFrames=4800;
    std::array<double,1500>sums{};std::array<uint64_t,1500>counts{};
    double momentary=-100,integrated=-100;
    explicit Loudness(double rate){blockFrames=(int)std::llround(rate*.1);for(int c=0;c<2;c++){k[c][0].set(4,1681.974450955533,.7071752369554196,3.999843853973347,rate);k[c][1].set(2,38.13547087602444,.5003270373238773,0,rate);}}
    void sample(double l,double r){double a=k[0][1].run(k[0][0].run(l)),b=k[1][1].run(k[1][0].run(r));energy[slot]+=a*a+b*b;
        if(++frames<blockFrames)return;frames=0;if(++parts>=4){double e=std::accumulate(energy.begin(),energy.end(),0.)/(4*blockFrames);momentary=e>1e-15?-.691+10*std::log10(e):-100;
            if(momentary>=-70){int bin=(int)clamp(std::floor((momentary+100)*10),0,1499);sums[bin]+=e;counts[bin]++;}
            double total=0;uint64_t n=0;for(int i=0;i<1500;i++){total+=sums[i];n+=counts[i];}double gate=n?std::max(-70.,-.691+10*std::log10(total/n)-10):-70;
            total=0;n=0;for(int i=0;i<1500;i++)if(-100+(i+.5)*.1>=gate){total+=sums[i];n+=counts[i];}integrated=n?-.691+10*std::log10(total/n):-100;
        }slot=(slot+1)%4;energy[slot]=0;
    }
};
struct TruePeak {
    double history[2][12]{};double kernel[4][12]{};int head=0;
    TruePeak(){for(int phase=0;phase<4;phase++){double total=0;for(int n=0;n<12;n++){double t=n-5-phase*.25;double sinc=std::abs(t)<1e-12?1:std::sin(pi*t)/(pi*t);double win=.42-.5*std::cos(2*pi*(n+.5)/12)+.08*std::cos(4*pi*(n+.5)/12);kernel[phase][n]=sinc*win;total+=kernel[phase][n];}for(auto&v:kernel[phase])v/=total;}}
    std::pair<double,double>sample(double l,double r){history[0][head]=l;history[1][head]=r;double peak[2]{std::abs(l),std::abs(r)};
        for(int c=0;c<2;c++)for(int phase=0;phase<4;phase++){double x=0;for(int n=0;n<12;n++)x+=history[c][(head+1+n)%12]*kernel[phase][n];peak[c]=std::max(peak[c],std::abs(x));}
        head=(head+1)%12;return{peak[0],peak[1]};}
};
double compression(double db,double threshold,double ratio,double knee){double over=db-threshold,change=1/ratio-1;if(knee>0&&std::abs(over)<knee*.5)return change*(over+knee*.5)*(over+knee*.5)/(2*knee);return over>0?change*over:0;}
}
struct NativeEffect::State {
    std::array<double,ParameterCapacity>smooth{},last{};bool coefficientDirty=true;std::array<double,16>reverbFeedback{};std::array<double,8>poleMs{},poleValue{};
    double cachedPole(int index,double ms,double rate){if(poleMs[index]!=ms){poleMs[index]=ms;poleValue[index]=pole(ms,rate);}return poleValue[index];}Biquad filters[10][2];double env[4]{},gain[4]{1,1,1,1},lp[4][2]{},ap[2][6]{},feedback[2]{},previous[2]{},dcIn[2]{},dcOut[2]{},phase=0,phaseR=0,hold=0,decimation=0,held[2]{};
    std::unique_ptr<NativePitch>pitch;std::unique_ptr<Delay>delay;std::vector<ReverbLine>combs,allpasses;std::unique_ptr<Loudness>meter;std::unique_ptr<TruePeak>truePeak;
    int limitHold[2]{},meterHold[2]{};double reduction=0;std::array<double,8>extra{};
};
NativeEffect::NativeEffect(int type,double sampleRate):kind(type){if(type<0||type>=(int)nativeSpecs().size())throw std::runtime_error("Unknown Vamp effect");for(size_t i=0;i<spec().parameters.size();i++)values[i]=spec().parameters[i].normalized(spec().parameters[i].initial);for(int i=0;i<ParameterCapacity;i++)bankA[i]=bankB[i]=values[i].load();if(kind==0)eqAnalysis=std::make_unique<EqAnalysis>();prepare(sampleRate);}
NativeEffect::~NativeEffect(){if(window)DestroyWindow(window);}
const NativeSpec&NativeEffect::spec()const{return nativeSpecs().at(kind);}
int NativeEffect::latency()const{return kind==28?2048:kind==31?(int)std::ceil(rate*.005)+12:0;}
bool NativeEffect::prepare(double sampleRate){if(sampleRate<8000||sampleRate>384000)return false;rate=sampleRate;dsp=std::make_unique<State>();auto&d=*dsp;for(int i=0;i<ParameterCapacity;i++)d.smooth[i]=values[i].load();d.last.fill(-99999);
    if(kind==13||(kind>=16&&kind<=22)||kind==28||kind==31||kind==40||kind==41)d.delay=std::make_unique<Delay>((size_t)(rate*(kind==16||kind==17||kind==41?4.1:.5))+32);
    if(kind>=18&&kind<=20){static int lengths[]={1116,1188,1277,1356,1422,1491,1557,1617};double size=kind==18?.45:kind==19?.85:1.3;for(int c=0;c<2;c++){for(int n:lengths)d.combs.emplace_back((size_t)((n+c*23)*rate/44100.*size));for(int n:{556,441,341,225})d.allpasses.emplace_back((size_t)((n+c*17)*rate/44100.*(kind==19?1.2:.8)));}}
    if(kind==46)assistantCapture=std::make_unique<AssistantCapture>(rate);if(kind==28)d.pitch=std::make_unique<NativePitch>();if(kind==29||kind==31)d.meter=std::make_unique<Loudness>(rate);if(kind==31)d.truePeak=std::make_unique<TruePeak>();momentary=integrated=-100;return true;
}
void NativeEffect::set(uint32_t id,double value,bool user){if(id>=spec().parameters.size()||!std::isfinite(value))return;values[id]=clamp(value,0,1);if(user)edited=true;}
Json NativeEffect::state()const{Json valuesJson=Json::array();for(size_t i=0;i<spec().parameters.size();i++)valuesJson.push_back(values[i].load());return{{"native",spec().id},{"version",2},{"parameters",valuesJson},{"quality",quality.load()},{"bankA",bankA},{"bankB",bankB},{"activeB",activeB},{"assistantAuto",assistantAuto},{"assistantAnyPlugins",assistantAnyPlugins},{"assistantOverrides",assistantOverrides},{"assistantReport",assistantReport}};}
void NativeEffect::restore(const Json&j){if(!j.is_object()||j.value("native","")!=spec().id)return;for(size_t i=0;i<spec().parameters.size();i++)set((uint32_t)i,spec().parameters[i].normalized(spec().parameters[i].initial));size_t bankLimit=kind==0&&j.value("version",1)<2?20:spec().parameters.size();if(j.contains("parameters")&&j["parameters"].is_array())for(size_t i=0;i<std::min(j["parameters"].size(),spec().parameters.size());i++)if(j["parameters"][i].is_number())set((uint32_t)i,j["parameters"][i].get<double>());quality=(int)clamp(j.value("quality",0),0,2);for(int i=0;i<ParameterCapacity;i++)bankA[i]=bankB[i]=values[i].load();for(auto pair:{std::make_pair("bankA",&bankA),std::make_pair("bankB",&bankB)})if(j.contains(pair.first)&&j[pair.first].is_array())for(size_t i=0;i<std::min(j[pair.first].size(),bankLimit);i++)if(j[pair.first][i].is_number()){double n=j[pair.first][i].get<double>();if(std::isfinite(n))(*pair.second)[i]=clamp(n,0,1);}assistantAuto=j.value("assistantAuto",true);assistantAnyPlugins=assistantStandalone||j.value("assistantAnyPlugins",false);assistantReport=j.value("assistantReport",Json::array());if(!assistantReport.is_array()||assistantReport.size()>16)assistantReport=Json::array();assistantOverrides=j.value("assistantOverrides",Json::object());if(!assistantOverrides.is_object())assistantOverrides=Json::object();activeB=j.value("activeB",false);if(window)InvalidateRect(window,nullptr,FALSE);}
double NativeEffect::eqResponseDb(double hz,int band)const{
    if(kind!=0)return 0;std::complex<double> transfer=1.;
    for(int n=0;n<7;n++)if(band<0||n==band){int base=eqBandBase(n);auto value=[&](int id){return spec().parameters[id].value(values[id].load(std::memory_order_relaxed));};NativeBiquad filter;filter.set(eqFilterType((int)value(23+n)),value(base),value(base+2),value(base+1),rate);transfer*=filter.response(hz,rate);}
    if(band<0){double wet=spec().parameters[18].value(values[18].load())*.01;transfer=(1-wet)+wet*transfer;transfer*=amp(spec().parameters[19].value(values[19].load()));}
    return 20*std::log10(std::max(1e-12,std::abs(transfer)));
}
void NativeEffect::process(float*l,float*r,int frames,double beat,double tempo,bool playing,const std::vector<Point>*automation,uint32_t parameter,const float*sideL,const float*sideR,const std::vector<AutomationLane>*lanes,double loopEnd,double loopStart){
    if(!dsp||frames<1)return;if(kind==46){if(assistantCapture)assistantCapture->process(l,r,frames);float peak=0;for(int i=0;i<frames;i++)peak=std::max({peak,std::abs(l[i]),std::abs(r[i])});inputPeak=outputPeak=peak;return;}auto&d=*dsp;const auto&parameters=spec().parameters;size_t count=parameters.size();std::array<double,ParameterCapacity>target{},v{};bool moving=false;bool automated=(automation&&!automation->empty())||(lanes&&!lanes->empty());for(size_t i=0;i<count;i++)target[i]=values[i].load(std::memory_order_relaxed);for(size_t i=0;i<count;i++)v[i]=parameters[i].value(d.smooth[i]);double smoothing=1-std::exp(-1/(rate*(kind==28?parameters[2].value(target[2])*.001:.004)));double inPeak=0,outPeak=0,maxReduction=0,sumL=0,sumR=0,cross=0;bool analyzeEq=eqAnalysis&&analyzerRunning.load(std::memory_order_relaxed);uint64_t eqHead=analyzeEq?eqAnalysis->head.load(std::memory_order_relaxed):0;
    double wet=kind==31?1:v[kind==0?18:count-2]*.01,output=kind==31?1:amp(v[kind==0?19:count-1]);
    for(int frame=0;frame<frames;frame++){
        double at=beat+(playing?frame*tempo/(60*rate):0);if(loopEnd>loopStart&&at>=loopEnd)at=loopStart+std::fmod(at-loopEnd,loopEnd-loopStart);
        if(frame==0||moving||automated){
        if(automation&&parameter<count&&!automation->empty())target[parameter]=automationAt(*automation,at);
        if(lanes)for(auto&lane:*lanes)if(lane.enabled&&lane.parameter<count&&!lane.points.empty())target[lane.parameter]=automationAt(lane.points,at);
        moving=false;for(size_t i=0;i<count;i++){moving|=std::abs(target[i]-d.smooth[i])>1e-10;d.smooth[i]+=smoothing*(target[i]-d.smooth[i]);if(std::abs(target[i]-d.smooth[i])<1e-10)d.smooth[i]=target[i];v[i]=parameters[i].value(parameters[i].stepped?target[i]:d.smooth[i]);if(std::abs(v[i]-d.last[i])>1e-5)d.coefficientDirty=true;}
        wet=kind==31?1:v[kind==0?18:count-2]*.01;output=kind==31?1:amp(v[kind==0?19:count-1]);
        }
        double x=std::isfinite(l[frame])?l[frame]:0,y=std::isfinite(r[frame])?r[frame]:0,a=x,b=y,dryL=x,dryR=y;inPeak=std::max(inPeak,std::max(std::abs(x),std::abs(y)));double detector=std::max(std::abs(x),std::abs(y));
        if((frame%32)==0&&d.coefficientDirty){
            if(kind==0)for(int band=0;band<7;band++){int base=eqBandBase(band);for(int c=0;c<2;c++)d.filters[band][c].set(eqFilterType((int)v[23+band]),v[base],v[base+2],v[base+1],rate);}
            if(kind==1)for(int c=0;c<2;c++)d.filters[0][c].set(3,v[0],v[6],0,rate);
            if(kind==2)for(int c=0;c<2;c++){d.filters[0][c].set(2,v[0],v[2],0,rate);d.filters[1][c].set(1,v[1],v[2],0,rate);}
            if(kind==9)for(int c=0;c<2;c++)d.filters[0][c].set(2,v[0],.707,0,rate);
            if(kind==13)for(int c=0;c<2;c++)d.filters[0][c].set(0,90,.8,v[2],rate);
            if(kind==30)for(int c=0;c<2;c++){d.filters[0][c].set(0,90,.6,v[1],rate);d.filters[1][c].set(0,10000,.6,v[2],rate);}
            if(kind>=18&&kind<=20){for(int c=0;c<2;c++)for(int n=0;n<8;n++){auto&line=d.combs[c*8+n];line.delay=line.base*v[3]*.01;d.reverbFeedback[c*8+n]=std::pow(.001,line.delay/(rate*v[0]));}for(auto&line:d.allpasses)line.delay=line.base*v[3]*.01;}
            #include "native_extra_coefficients.inc"
            d.last=v;d.coefficientDirty=false;
        }
        if(kind==0){for(int n=0;n<7;n++){a=d.filters[n][0].run(a);b=d.filters[n][1].run(b);}}
        else if(kind==1){double bandL=d.filters[0][0].run(x),bandR=d.filters[0][1].run(y),det=std::max(std::abs(bandL),std::abs(bandR));double k=det>d.env[0]?d.cachedPole(0,v[4],rate):d.cachedPole(1,v[5],rate);d.env[0]=k*d.env[0]+(1-k)*det;double gr=std::max(v[1],compression(level(d.env[0]),v[2],v[3],3));double g=amp(gr);a=x+bandL*(g-1);b=y+bandR*(g-1);maxReduction=std::max(maxReduction,-gr);}
        else if(kind==2){a=d.filters[1][0].run(d.filters[0][0].run(a));b=d.filters[1][1].run(d.filters[0][1].run(b));}
        else if(kind>=3&&kind<=5){if(v[5]>.5)detector=sideL?std::max(std::abs(sideL[frame]),std::abs(sideR?sideR[frame]:sideL[frame])):0;double det=kind==5?detector*detector:detector;double release=v[3]*(kind==5?(1+2*clamp(d.env[0],0,1)):1),k=det>d.env[0]?d.cachedPole(0,v[2],rate):d.cachedPole(1,release,rate);d.env[0]=k*d.env[0]+(1-k)*det;double gr=compression(level(kind==5?std::sqrt(d.env[0]):d.env[0]),v[0],v[1],v[6]);double g=amp(gr+v[4]);a=x*g;b=y*g;if(kind==4){a=std::tanh(a*1.15)/1.15;b=std::tanh(b*1.15)/1.15;}maxReduction=std::max(maxReduction,-gr);}
        else if(kind==6){double c0=1-std::exp(-2*pi*v[0]/rate),c1=1-std::exp(-2*pi*v[1]/rate);double bands[3][2];for(int c=0;c<2;c++){double in=c?y:x;d.lp[0][c]+=c0*(in-d.lp[0][c]);d.lp[1][c]+=c1*(in-d.lp[1][c]);bands[0][c]=d.lp[0][c];bands[1][c]=d.lp[1][c]-d.lp[0][c];bands[2][c]=in-d.lp[1][c];}a=b=0;for(int n=0;n<3;n++){double det=std::max(std::abs(bands[n][0]),std::abs(bands[n][1])),k=det>d.env[n]?d.cachedPole(0,v[6],rate):d.cachedPole(1,v[7],rate);d.env[n]=k*d.env[n]+(1-k)*det;double gr=compression(level(d.env[n]),v[2+n],v[5],6),g=amp(gr+v[8+n]);a+=bands[n][0]*g;b+=bands[n][1]*g;maxReduction=std::max(maxReduction,-gr);}}
        else if(kind==7||kind==8){if(v[5]>.5)detector=sideL?std::max(std::abs(sideL[frame]),std::abs(sideR?sideR[frame]:sideL[frame])):0;double db=level(detector),targetGain=1;if(kind==7){if(db>=v[0])d.hold=v[2]*.001*rate;else if(d.hold>0)d.hold--;else targetGain=amp(v[4]);}else targetGain=amp(std::max(v[4],std::min(0.,(db-v[0])*(v[1]-1))));double k=targetGain>d.gain[0]?d.cachedPole(0,v[kind==7?1:2],rate):d.cachedPole(1,v[3],rate);d.gain[0]=k*d.gain[0]+(1-k)*targetGain;a=x*d.gain[0];b=y*d.gain[0];maxReduction=std::max(maxReduction,-level(d.gain[0]));}
        else if(kind==9){double hiL=d.filters[0][0].run(x),hiR=d.filters[0][1].run(y),det=std::max(std::abs(hiL),std::abs(hiR)),k=det>d.env[0]?d.cachedPole(0,v[3],rate):d.cachedPole(1,v[4],rate);d.env[0]=k*d.env[0]+(1-k)*det;double gr=std::max(v[2],compression(level(d.env[0]),v[1],6,3)),g=amp(gr);a=x+hiL*(g-1);b=y+hiR*(g-1);maxReduction=std::max(maxReduction,-gr);}
        else if(kind==10){double fast=detector>d.env[0]?d.cachedPole(0,1,rate):d.cachedPole(1,25,rate),slow=detector>d.env[1]?d.cachedPole(2,25,rate):d.cachedPole(3,200,rate);d.env[0]=fast*d.env[0]+(1-fast)*detector;d.env[1]=slow*d.env[1]+(1-slow)*detector;double transient=clamp((d.env[0]-d.env[1])*v[2]/(d.env[1]+.03),-1,1),db=v[0]*.12*std::max(0.,transient)+v[1]*.12*(1-std::max(0.,transient));a=x*amp(db);b=y*amp(db);}
        else if(kind>=11&&kind<=14){int oversample=1<<quality.load();double drive=amp(v[0]),ceiling=kind==11?amp(v[1]):1,bias=kind==12?v[1]*.01:kind==14?v[1]*.006:0;double out[2]{};
            for(int c=0;c<2;c++){double in=c?y:x;for(int n=1;n<=oversample;n++){double z=(d.previous[c]+(in-d.previous[c])*n/oversample)*drive;
                if(kind==11){double hard=clamp(z,-ceiling,ceiling),soft=ceiling*std::tanh(z/ceiling);z=hard+(soft-hard)*v[2]*.01;}
                else {double scale=kind==13?.85:1;z=(std::tanh((z+bias)*scale)-std::tanh(bias*scale))/scale;}
                if(oversample>1){double k=1-std::exp(-2*pi*std::min(20000.,rate*.42)/(rate*oversample));d.lp[2][c]+=k*(z-d.lp[2][c]);d.lp[3][c]+=k*(d.lp[2][c]-d.lp[3][c]);z=d.lp[3][c];}out[c]+=z/oversample;
            }d.previous[c]=in;}
            a=out[0];b=out[1];if(kind!=11){double hz=kind==13?v[1]:v[2],k=1-std::exp(-2*pi*std::min(hz,rate*.46)/rate);for(int c=0;c<2;c++){double z=c?b:a;d.lp[0][c]+=k*(z-d.lp[0][c]);double dc=d.lp[0][c]-d.dcIn[c]+.997*d.dcOut[c];d.dcIn[c]=d.lp[0][c];d.dcOut[c]=dc;if(c)b=dc;else a=dc;}}
            if(kind==13){a=d.filters[0][0].run(a);b=d.filters[0][1].run(b);d.delay->write(a,b);double wobble=v[3]*.000015*rate*(1+std::sin(d.phase));a=d.delay->read(0,2+wobble);b=d.delay->read(1,2+wobble);d.phase=std::fmod(d.phase+2*pi*.6/rate,2*pi);}}
        else if(kind==15){d.decimation+=std::min(rate,v[1])/rate;if(d.decimation>=1){d.decimation-=1;double scale=std::pow(2.,v[0]-1)-1;d.held[0]=std::round(clamp(x*amp(v[2]),-1,1)*scale)/scale;d.held[1]=std::round(clamp(y*amp(v[2]),-1,1)*scale)/scale;}a=d.held[0];b=d.held[1];}
        else if(kind==16||kind==17){double seconds=v[3]>.5?v[4]*60/std::max(1.,tempo):v[0]*.001;seconds=clamp(seconds,.001,4);a=d.delay->read(0,seconds*rate);b=d.delay->read(1,seconds*rate);double k=1-std::exp(-2*pi*std::min(v[2],rate*.46)/rate);d.lp[0][0]+=k*(a-d.lp[0][0]);d.lp[0][1]+=k*(b-d.lp[0][1]);double feedback=v[1]*.01;d.delay->write(x+(kind==17?d.lp[0][1]:d.lp[0][0])*feedback,y+(kind==17?d.lp[0][0]:d.lp[0][1])*feedback);}
        else if(kind>=18&&kind<=20){double mono=(x+y)*.5;d.delay->write(mono,mono);double predelay=d.delay->read(0,std::max(1.,v[2]*.001*rate)),k=1-std::exp(-2*pi*std::min(v[1],rate*.46)/rate);double out[2]{};int lines=quality.load()?8:4;
            for(int c=0;c<2;c++){double z=predelay;for(int i=0;i<4;i++)z=d.allpasses[c*4+i].allpass(z);for(int i=0;i<lines;i++){auto&comb=d.combs[c*8+i];double fb=d.reverbFeedback[c*8+i];out[c]+=comb.comb(z,fb,k)/lines;}if(kind==20)out[c]*=.96+.04*std::sin(d.phase+c*1.2);}
            double mid=(out[0]+out[1])*.5,side=(out[0]-out[1])*.5*v[4]*.01;a=mid+side;b=mid-side;d.phase=std::fmod(d.phase+2*pi*.21/rate,2*pi);}
        else if(kind==21||kind==22){double depth=v[1]*.01,base=kind==21?.018:.002,range=kind==21?.009:.0018;double dl=rate*(base+range*depth*std::sin(d.phase)),dr=rate*(base+range*depth*std::sin(d.phase+v[3]*pi/180));a=d.delay->read(0,dl);b=d.delay->read(1,dr);d.delay->write(x+a*v[2]*.01,y+b*v[2]*.01);d.phase=std::fmod(d.phase+2*pi*v[0]/rate,2*pi);}
        else if(kind==23){double result[2];for(int c=0;c<2;c++){double hz=std::pow(2.,std::sin(d.phase+c*v[3]*pi/180)*v[1]*.035)*700,h=std::tan(pi*clamp(hz,30,rate*.4)/rate),coef=(h-1)/(h+1),z=(c?y:x)+d.feedback[c]*v[2]*.01;for(int n=0;n<6;n++){double next=coef*z+d.ap[c][n];d.ap[c][n]=z-coef*next;z=next;}d.feedback[c]=z;result[c]=z;}a=result[0];b=result[1];d.phase=std::fmod(d.phase+2*pi*v[0]/rate,2*pi);}
        else if(kind==24||kind==25){double hz=v[3]>.5?tempo/(60*v[4]):v[0];double wave=std::sin(d.phase),shaped=std::tanh(wave*(1+v[2]*.09))/std::tanh(1+v[2]*.09);if(kind==24){double g=1-v[1]*.005*(1+shaped);a=x*g;b=y*g;}else{double pan=shaped*v[1]*.01;a=x*std::cos((pan+1)*pi*.25)*1.41421356237;b=y*std::sin((pan+1)*pi*.25)*1.41421356237;}d.phase=std::fmod(d.phase+2*pi*hz/rate,2*pi);}
        else if(kind==26){double mid=(x+y)*.5,side=(x-y)*.5,k=1-std::exp(-2*pi*v[1]/rate);d.lp[0][0]+=k*(side-d.lp[0][0]);side=(side-d.lp[0][0])*v[0]*.01*amp(v[3]);mid*=amp(v[2]);a=mid+side;b=mid-side;}
        else if(kind==27){if(v[5]>.5)std::swap(a,b);if(v[2]>.5)a=b=(a+b)*.5;if(v[3]>.5)a=-a;if(v[4]>.5)b=-b;a*=amp(v[0])*(v[1]>0?1-v[1]*.01:1);b*=amp(v[0])*(v[1]<0?1+v[1]*.01:1);}
        else if(kind==28){dryL=d.delay->read(0,latency());dryR=d.delay->read(1,latency());double ratioL=std::pow(2.,(v[0]+v[1]/100-v[3]/200)/12.),ratioR=std::pow(2.,(v[0]+v[1]/100+v[3]/200)/12.);auto shifted=d.pitch->sample(x,y,ratioL,ratioR);a=shifted.first;b=shifted.second;d.delay->write(x,y);if(std::abs(ratioL-1)<1e-8)a=dryL;if(std::abs(ratioR-1)<1e-8)b=dryR;}
        else if(kind==30){if(v[0]>.5){a=d.filters[1][0].run(d.filters[0][0].run(a));b=d.filters[1][1].run(d.filters[0][1].run(b));}if(v[3]>.5){double det=std::max(std::abs(a),std::abs(b)),k=det>d.env[0]?d.cachedPole(0,30,rate):d.cachedPole(1,250,rate);d.env[0]=k*d.env[0]+(1-k)*det;double gr=compression(level(d.env[0]),v[4],v[5],6);a*=amp(gr);b*=amp(gr);maxReduction=std::max(maxReduction,-gr);}if(v[6]>.5){double drive=amp(v[7]);a=std::tanh(a*drive)/drive;b=std::tanh(b*drive)/drive;}if(v[8]>.5){double mid=(a+b)*.5,side=(a-b)*.5*v[9]*.01;a=mid+side;b=mid-side;}}
        else if(kind==31){double gain=amp(v[0]),ceiling=amp(v[1]-1.2);a=x*gain;b=y*gain;auto peaks=d.truePeak->sample(a,b);double link=v[3]*.01,maximum=std::max(peaks.first,peaks.second),out[2];dryL=d.delay->read(0,latency());dryR=d.delay->read(1,latency());
            for(int c=0;c<2;c++){double peak=(c?peaks.second:peaks.first)*(1-link)+maximum*link,g=std::min(1.,ceiling/std::max(1e-12,peak));if(g<d.gain[c]){d.gain[c]=g;d.limitHold[c]=latency()+12;}else if(d.limitHold[c]>0)d.limitHold[c]--;else{double k=d.cachedPole(0,v[2],rate);d.gain[c]=k*d.gain[c]+(1-k)*g;}out[c]=d.delay->read(c,latency())*d.gain[c];maxReduction=std::max(maxReduction,-level(d.gain[c]));}d.delay->write(a,b);a=clamp(out[0],-ceiling,ceiling);b=clamp(out[1],-ceiling,ceiling);}
        #include "native_extra_process.inc"
        a=(dryL+(a-dryL)*wet)*output;b=(dryR+(b-dryR)*wet)*output;if(!std::isfinite(a))a=0;if(!std::isfinite(b))b=0;l[frame]=(float)a;r[frame]=(float)b;outPeak=std::max(outPeak,std::max(std::abs(a),std::abs(b)));
        if(analyzeEq){eqAnalysis->input[eqHead%4096].store((float)x,std::memory_order_relaxed);eqAnalysis->inputR[eqHead%4096].store((float)y,std::memory_order_relaxed);eqAnalysis->output[eqHead%4096].store((float)a,std::memory_order_relaxed);eqAnalysis->outputR[eqHead%4096].store((float)b,std::memory_order_relaxed);eqHead++;}
        if(kind==29){sumL+=a*a;sumR+=b*b;cross+=a*b;}if(d.meter)d.meter->sample(a,b);
        if(kind==29){uint64_t head=scopeHead.load(std::memory_order_relaxed);scope[head%1024].store((float)((a+b)*.5),std::memory_order_relaxed);scopeHead.store(head+1,std::memory_order_release);}
    }
    if(analyzeEq)eqAnalysis->head.store(eqHead,std::memory_order_release);
    if(kind==29){rms=(float)std::sqrt((sumL+sumR)/(2*frames));correlation=(float)(cross/std::max(1e-20,std::sqrt(sumL*sumR)));}
    if(d.meter){momentary=d.meter->momentary;integrated=d.meter->integrated;}
    if(kind==29){for(int c=0;c<2;c++){auto&meter=c?outputPeak:inputPeak;double current=c?outPeak:inPeak;if(current>=meter.load()){meter=(float)current;d.meterHold[c]=(int)(v[1]*rate);}else if((d.meterHold[c]-=frames)<=0)meter=(float)std::max(current,meter.load()*std::exp(-frames/(rate*.3)));}}else{inputPeak.store((float)std::max(inPeak,inputPeak.load()*.9));outputPeak.store((float)std::max(outPeak,outputPeak.load()*.9));}reduction.store((float)maxReduction);
}
}
