#include "assistant.hpp"
#include <complex>
#include <numeric>
namespace vamp {
const std::vector<AssistantPreset>&assistantPresets(){
    static const std::vector<AssistantPreset>p={
        {"Vocal / Natural lead",false,true,75,-2,1,2.5,18,160,-20,8},
        {"Vocal / Bright pop",false,true,90,-2,2.5,3,10,120,-18,10,5},
        {"Vocal / Dense rap",false,true,80,-2,1,4,6,100,-18,3,4,1},
        {"Vocal / Soft intimate",false,true,65,-1,1,2,30,250,-22,7},
        {"Vocal / Rock presence",false,true,85,-2,1.5,4,12,120,-18,6,0,2},
        {"Vocal / Airy backing",false,true,140,-2,3,3,15,200,-24,18,4,0,115},
        {"Vocal / Sibilance control",false,true,70,-1,0,2,20,180,-20},
        {"Vocal / Noisy room",false,true,100,-2,0,2.5,15,180,-22},
        {"Drums / Punchy kick",false,false,28,-2,0,3,30,100,-20,0,0,1},
        {"Drums / Snare body",false,false,90,-1,1,3,25,120,-20,7,0,1},
        {"Drums / Smooth cymbals",false,false,350,-1,-1,1.5,15,100,-26,4},
        {"Drums / Parallel room",false,false,45,-2,1,4,30,160,-20,12,0,1,110},
        {"Bass / Clean sub",false,false,25,-2,0,3,25,180,-22,0,0,0,0},
        {"Bass / Electric finger",false,false,35,-2,1,3.5,20,150,-20,0,0,1,0},
        {"Bass / Aggressive synth",false,false,28,-2,1,4,12,100,-20,0,0,3,70},
        {"Guitar / Acoustic",false,false,75,-2,1.5,2,25,200,-23,8},
        {"Guitar / Clean electric",false,false,90,-2,1,2.5,20,180,-22,8,5},
        {"Guitar / Heavy rhythm",false,false,90,-3,0,2,30,130,-22,0,0,0,100},
        {"Keys / Piano",false,false,45,-1,1,1.7,35,240,-24,10},
        {"Synth / Lead",false,false,110,-2,1.5,2.5,15,130,-22,6,8,1},
        {"Synth / Wide pad",false,false,150,-2,1,1.5,40,300,-26,18,0,0,115},
        {"Strings / Gentle ensemble",false,false,65,-1,1,1.5,45,280,-25,12},
        {"Bus / Drum glue",false,false,30,-1,0,2,30,180,-22,0,0,1},
        {"Bus / Vocal group",false,true,65,-1,1,2,25,200,-22,5},
        {"Bus / Music glue",false,false,30,-1,0,1.5,35,220,-22},
        {"Master / Balanced streaming",true,false,25,-.7,.5,1.5,30,250,-16},
        {"Master / Loud electronic",true,false,28,-1,1,1.8,25,150,-12,0,0,1,105},
        {"Master / Dynamic acoustic",true,false,22,-.5,.3,1.3,50,350,-19},
        {"Master / Warm rock",true,false,25,-1,.3,1.7,30,200,-14,0,0,1},
        {"Master / Transparent premaster",true,false,22,-.3,0,1.2,45,300,-21}
    };return p;
}
static double dbAssistant(double x){return 20*std::log10(std::max(1e-12,x));}
void AssistantCapture::process(const float*l,const float*r,int frames){
    int status=state.load(std::memory_order_acquire);if(status!=1&&status!=2)return;
    if(status==1){written=0;started=false;progress=0;state.store(2,std::memory_order_release);}
    if(!started){for(int i=0;i<frames;i++)if(std::abs(l[i])>1e-5||std::abs(r[i])>1e-5){started=true;break;}if(!started)return;}
    size_t target=targetFrames.load(std::memory_order_relaxed),count=std::min<size_t>(frames,target-written);for(size_t i=0;i<count;i++){left[written+i]=std::isfinite(l[i])?l[i]:0;right[written+i]=std::isfinite(r[i])?r[i]:0;}
    written+=count;progress.store(written,std::memory_order_relaxed);if(written==target)state.store(3,std::memory_order_release);
}
AssistantAnalysis analyzeAssistant(const float*l,const float*r,size_t frames,double rate){
    AssistantAnalysis a;if(!l||!r||rate<8000||frames<(size_t)(rate*.25))return a;
    double energy=0,el=0,er=0,cross=0,peak=0,sumL=0,sumR=0,clips=0;std::vector<double>windows;
    size_t block=std::max<size_t>(1,(size_t)(rate*.02));double part=0;
    for(size_t i=0;i<frames;i++){double x=std::isfinite(l[i])?l[i]:0,y=std::isfinite(r[i])?r[i]:0;el+=x*x;er+=y*y;cross+=x*y;sumL+=x;sumR+=y;peak=std::max({peak,std::abs(x),std::abs(y)});part+=(x*x+y*y)*.5;if(std::abs(x)>=.999||std::abs(y)>=.999)clips++;if((i+1)%block==0){windows.push_back(dbAssistant(std::sqrt(part/block)));energy+=part;part=0;}}
    energy+=part;a.seconds=frames/rate;a.peak=dbAssistant(peak);a.rms=dbAssistant(std::sqrt(energy/frames));a.valid=a.rms>-90&&peak>1e-5;if(!a.valid)return a;
    a.crest=a.peak-a.rms;a.dc=std::max(std::abs(sumL),std::abs(sumR))/frames;a.clipped=clips/frames;a.correlation=cross/std::max(1e-20,std::sqrt(el*er));
    std::sort(windows.begin(),windows.end());a.noise=windows.empty()?-120:windows[windows.size()/10];
    // Stereo power spectra keep anti-phase content; a mono sum would erase it.
    constexpr size_t N=2048;std::array<double,N/2>power{};std::array<std::complex<double>,N>fft;
    for(size_t frame=0;frame<48;frame++)for(int channel=0;channel<2;channel++){
        size_t start=(frames>N?(frames-N)*frame/47:0);auto*source=channel?r:l;
        for(size_t i=0;i<N;i++)fft[i]=(start+i<frames?source[start+i]:0)*(.5-.5*std::cos(6.283185307179586*i/(N-1)));
        for(size_t i=1,j=0;i<N;i++){size_t bit=N/2;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(fft[i],fft[j]);}
        for(size_t length=2;length<=N;length*=2){auto step=std::polar(1.,-6.283185307179586/length);for(size_t i=0;i<N;i+=length){std::complex<double>w=1;for(size_t j=0;j<length/2;j++){auto x=fft[i+j],y=w*fft[i+j+length/2];fft[i+j]=x+y;fft[i+j+length/2]=x-y;w*=step;}}}
        for(size_t i=1;i<N/2;i++)power[i]+=std::norm(fft[i]);
    }
    double total=std::accumulate(power.begin()+1,power.end(),0.);for(size_t i=1;i<N/2;i++){double hz=i*rate/N,e=power[i]/std::max(1e-24,total);a.centroid+=e*hz;if(hz<60)a.sub+=e;else if(hz<200)a.bass+=e;else if(hz<600)a.mud+=e;else if(hz>=2500&&hz<7000)a.presence+=e;else if(hz>=7000)a.air+=e;}
    return a;
}
int detectAssistantPreset(const AssistantAnalysis&a,bool master){if(master)return a.crest>16?27:25;if(a.sub+a.bass>.7)return a.crest>15?8:12;if(a.crest>18)return 11;if(a.centroid>7000)return 10;if(a.centroid>2600)return 19;return 24;}
AssistantPlan makeAssistantPlan(int preset,double strength,const AssistantAnalysis*analysis){
    AssistantPlan plan;plan.preset=(int)clamp(preset,0,assistantPresets().size()-1);plan.strength=clamp(strength,0,100);auto&p=assistantPresets()[plan.preset];double amount=plan.strength*.01;
    if(analysis&&analysis->valid){plan.analyzed=true;plan.analysis=*analysis;}auto&a=plan.analysis;
    auto add=[&](int kind,const char*reason,std::vector<std::pair<int,double>>settings){plan.steps.push_back({kind,reason,std::move(settings)});};
    if(plan.analyzed&&a.dc>.002)add(32,"Remove measured DC offset",{{0,10}});
    double hp=p.highpass;if(plan.analyzed&&a.sub>.35&&hp>60)hp=std::max(35.,hp*.7);
    add(2,"Remove rumble below the useful range",{{0,hp},{1,p.mastering?21000:20000}});
    if(preset==7)add(8,"Reduce quiet room bleed; not spectral denoising",{{0,plan.analyzed?clamp(a.noise+5,-65,-30):-48},{1,1.6},{4,-12*amount}});
    double mud=p.mud*amount,air=p.air*amount;if(plan.analyzed){mud=clamp(mud-(a.mud>.35?2*amount:0),-4,0);if(a.air>.18)air=std::min(air,0.);else if(a.air<.01&&a.centroid>400)air=std::min(3.,air+amount);}
    add(0,"Shape low mids and high-frequency balance",{{3,320},{4,mud},{5,.8},{15,11000},{16,air},{17,.7},{28,2}});
    if(p.vocal&&(preset==6||!plan.analyzed||a.presence+a.air>.18))add(9,"Control sibilance when high-frequency energy is elevated",{{0,6500},{1,plan.analyzed?clamp(a.rms-8,-45,-18):-28},{2,-(3+5*amount)}});
    if(p.vocal&&(preset==7||(plan.analyzed&&a.sub>.12)))add(37,"Tame low-frequency vocal bursts",{{0,160},{1,plan.analyzed?clamp(a.rms-4,-40,-12):-24},{2,-8*amount}});
    double ratio=1+(p.ratio-1)*amount,threshold=plan.analyzed?clamp(a.rms+3,-36,-8):p.target+5;
    if(!plan.analyzed||a.crest>7)add(3,"Control dynamics with scenario-specific timing",{{0,threshold},{1,ratio},{2,p.attack},{3,p.release},{4,0}});
    if(p.drive>0)add(12,"Add a restrained harmonic layer",{{0,p.drive*amount},{3,p.mastering?12:25}});
    if(p.width!=100)add(26,"Set width and keep the low band mono",{{0,plan.analyzed&&a.correlation<0?std::min(85.,p.width):100+(p.width-100)*amount},{1,p.mastering?110:90}});
    else if(plan.analyzed&&a.correlation<-.15)add(26,"Reduce measured negative stereo correlation",{{0,75},{1,100}});
    if(p.delay>0)add(41,"Add a tempo-synced delay that ducks behind the source",{{1,25},{3,1},{4,.5},{8,p.delay*amount}});
    if(p.reverb>0)add(18,"Place the source in a short room",{{0,p.vocal?.9:1.2},{2,18},{5,p.reverb*amount}});
    if(p.mastering){double input=plan.analyzed?clamp(p.target-a.rms,0,6)*amount:0;add(31,"Conservative ceiling; verify final loudness by listening",{{0,input},{1,-1},{2,180}});}
    else add(27,"Leave conservative headroom after the chain",{{0,plan.analyzed?clamp(p.target-a.rms,-6,3)*amount:0}});
    return plan;
}
}
