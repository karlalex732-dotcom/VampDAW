#include "native_fx.hpp"
#include <stdexcept>
namespace vamp {
double NativeParameter::value(double n)const{n=clamp(n,0,1);double v=logarithmic?low*std::pow(high/low,n):low+n*(high-low);return stepped?std::round(v):v;}
double NativeParameter::normalized(double v)const{v=clamp(v,low,high);return clamp(logarithmic?std::log(v/low)/std::log(high/low):(v-low)/(high-low),0,1);}
static NativeParameter p(const char*n,const char*u,double lo,double hi,double init,bool log=false,bool steps=false){return {n,u,lo,hi,init,log,steps};}
const std::vector<NativeSpec>&nativeSpecs(){
    static const auto result=[] {
        std::vector<NativeSpec>s;
        auto add=[&](const char*id,const char*name,const char*category,const char*description,std::vector<NativeParameter>parameters){parameters.push_back(p("Mix","%",0,100,100));parameters.push_back(p("Output","dB",-24,12,0));s.push_back({id,name,category,description,std::move(parameters)});};
        std::vector<NativeParameter>eq;for(int i=0;i<6;i++){auto f=p("Frequency","Hz",20,20000,std::array<double,6>{60,180,600,2000,6000,14000}[i],true);f.name="Band "+std::to_string(i+1)+" / Frequency";eq.push_back(f);auto gain=p("Gain","dB",-18,18,0);gain.name="Band "+std::to_string(i+1)+" / Gain";eq.push_back(gain);auto q=p("Q","",.2,12,.707,true);q.name="Band "+std::to_string(i+1)+" / Q";eq.push_back(q);}
        add("blood-eq","Blood EQ","EQ","Seven visual parametric bands with live spectrum and filter types",eq);
        // Keep IDs 0..19 stable for existing six-band alpha sessions and automation.
        s[0].parameters.push_back(p("Band 7 / Frequency","Hz",20,20000,16000,true));
        s[0].parameters.push_back(p("Band 7 / Gain","dB",-18,18,0));
        s[0].parameters.push_back(p("Band 7 / Q","",.2,12,.707,true));
        for(int band=0;band<7;band++){auto mode=p("Type","",0,7,0,false,true);mode.name="Band "+std::to_string(band+1)+" / Type";s[0].parameters.push_back(mode);}
        add("veil-dynamic-eq","Veil Dynamic EQ","EQ","Level-dependent bell cut with attack and release",{p("Frequency","Hz",40,16000,2800,true),p("Range","dB",-18,0,-6),p("Threshold","dB",-60,0,-24),p("Ratio","",1,12,3),p("Attack","ms",.1,200,5,true),p("Release","ms",10,1500,120,true),p("Q","",.3,8,1,true)});
        add("night-filter","Night Filter","Filter","Cascaded 12 dB/oct high-pass and low-pass",{p("High-pass","Hz",20,2000,20,true),p("Low-pass","Hz",200,22000,20000,true),p("Resonance","Q",.5,4,.707,true)});
        auto comp=std::vector<NativeParameter>{p("Threshold","dB",-60,0,-18),p("Ratio","",1,20,4,true),p("Attack","ms",.1,200,15,true),p("Release","ms",10,2000,160,true),p("Makeup","dB",-12,24,0),p("External sidechain","",0,1,0,false,true),p("Knee","dB",0,24,6)};
        add("fang-compressor","Fang Compressor","Dynamics","Stereo-linked soft-knee compressor with external sidechain",comp);
        auto fet=comp;fet[2]=p("Attack","ms",.02,20,.3,true);fet[3]=p("Release","ms",20,1100,80,true);fet[1].initial=8;add("fang-fet","Fang FET","Dynamics","Fast peak compression with driven FET-style coloration",fet);
        auto opto=comp;opto[2].initial=30;opto[3].initial=450;opto[1].initial=3;add("velvet-opto","Velvet Opto","Dynamics","RMS detector and program-dependent optical-style recovery",opto);
        add("triad-multiband","Triad Multiband","Dynamics","Three complementary crossover bands with independent compression",{p("Low crossover","Hz",60,800,180,true),p("High crossover","Hz",1000,12000,3500,true),p("Low threshold","dB",-48,0,-18),p("Mid threshold","dB",-48,0,-18),p("High threshold","dB",-48,0,-18),p("Ratio","",1,12,3,true),p("Attack","ms",.1,100,10,true),p("Release","ms",20,1200,180,true),p("Low gain","dB",-12,12,0),p("Mid gain","dB",-12,12,0),p("High gain","dB",-12,12,0)});
        add("crypt-gate","Crypt Gate","Dynamics","Stereo-linked gate with hold and adjustable range",{p("Threshold","dB",-80,0,-42),p("Attack","ms",.1,100,2,true),p("Hold","ms",0,500,40),p("Release","ms",5,1500,150,true),p("Range","dB",-96,0,-80),p("External sidechain","",0,1,0,false,true)});
        add("veil-expander","Veil Expander","Dynamics","Downward expansion with soft gain recovery",{p("Threshold","dB",-80,0,-38),p("Ratio","",1,8,2,true),p("Attack","ms",.1,100,4,true),p("Release","ms",10,1500,180,true),p("Range","dB",-80,0,-36),p("External sidechain","",0,1,0,false,true)});
        add("silk-de-esser","Silk De-Esser","Vocal","Split-band sibilance reduction",{p("Frequency","Hz",2000,12000,6500,true),p("Threshold","dB",-60,0,-28),p("Range","dB",-18,0,-9),p("Attack","ms",.1,30,1,true),p("Release","ms",10,500,80,true)});
        add("claw-transient","Claw Transient","Dynamics","Fast and slow envelope transient shaping",{p("Attack","%",-100,100,25),p("Sustain","%",-100,100,0),p("Sensitivity","",.1,4,1,true)});
        add("obsidian-clipper","Obsidian Clipper","Distortion","Soft or hard clip with drive and ceiling",{p("Drive","dB",0,30,3),p("Ceiling","dB",-18,0,-1),p("Softness","%",0,100,35)});
        add("blood-saturator","Blood Saturator","Distortion","Symmetric saturation with adjustable bias",{p("Drive","dB",0,30,6),p("Bias","%",-50,50,0),p("Tone","Hz",500,22000,18000,true)});
        add("nocturnal-tape","Nocturnal Tape","Distortion","Saturation, head bump, roll-off and wow",{p("Drive","dB",0,24,4),p("Tone","Hz",2000,20000,14000,true),p("Head bump","dB",0,9,1),p("Wow","%",0,100,8)});
        add("crimson-tube","Crimson Tube","Distortion","Asymmetric tube-style harmonics and tone filtering",{p("Drive","dB",0,30,5),p("Bias","%",0,100,25),p("Tone","Hz",1000,22000,16000,true)});
        add("grave-crusher","Grave Crusher","Distortion","Sample-and-hold decimation and quantization",{p("Bit depth","bit",2,24,12,false,true),p("Sample rate","Hz",1000,48000,16000,true),p("Drive","dB",0,18,0)});
        auto delay=std::vector<NativeParameter>{p("Time","ms",1,2000,375,true),p("Feedback","%",0,94,35),p("Low-pass","Hz",500,20000,7000,true),p("Tempo sync","",0,1,1,false,true),p("Division","beats",.125,4,1,true)};
        add("echo-delay","Echo Delay","Delay","Filtered stereo echo with tempo sync",delay);add("twin-echo","Twin Echo","Delay","Cross-feedback ping-pong delay",delay);
        auto reverb=std::vector<NativeParameter>{p("Decay","s",.15,12,1.2,true),p("Damping","Hz",500,18000,6500,true),p("Pre-delay","ms",0,200,12),p("Size","%",35,150,80),p("Width","%",0,150,100)};
        add("crypt-room","Crypt Room","Reverb","Short diffused room reflections",reverb);reverb[0].initial=2;reverb[2].initial=24;add("silver-plate","Silver Plate","Reverb","Dense plate-style reverb with all-pass diffusion",reverb);reverb[0].initial=3.5;reverb[3].initial=115;add("cathedral-hall","Cathedral Hall","Reverb","Long modulated stereo hall",reverb);
        auto mod=std::vector<NativeParameter>{p("Rate","Hz",.05,10,.7,true),p("Depth","%",0,100,55),p("Feedback","%",-90,90,10),p("Stereo phase","deg",0,180,90)};
        add("mist-chorus","Mist Chorus","Modulation","Dual fractional modulated delays",mod);mod[0].initial=.2;mod[2].initial=55;add("wing-flanger","Wing Flanger","Modulation","Short swept comb delay with feedback",mod);mod[2].initial=20;add("moon-phaser","Moon Phaser","Modulation","Six-stage stereo all-pass sweep",mod);
        auto rhythmic=std::vector<NativeParameter>{p("Rate","Hz",.05,20,4,true),p("Depth","%",0,100,75),p("Shape","%",0,100,0),p("Tempo sync","",0,1,1,false,true),p("Division","beats",.125,4,1,true)};
        add("pulse-tremolo","Pulse Tremolo","Modulation","Tempo-synced gain modulation",rhythmic);add("orbit-pan","Orbit Pan","Modulation","Constant-power automatic panning",rhythmic);
        add("night-width","Night Width","Stereo","Mid/Side width with a mono low-frequency band",{p("Width","%",0,200,100),p("Mono bass","Hz",20,500,100,true),p("Mid gain","dB",-12,12,0),p("Side gain","dB",-24,12,0)});
        add("vamp-utility","Vamp Utility","Utility","Gain, balance, mono, polarity and channel swap",{p("Gain","dB",-48,24,0),p("Balance","%",-100,100,0),p("Mono","",0,1,0,false,true),p("Invert left","",0,1,0,false,true),p("Invert right","",0,1,0,false,true),p("Swap channels","",0,1,0,false,true)});
        add("shade-pitch","Shade Pitch","Pitch","Streaming spectral pitch shift; no formant correction",{p("Semitones","st",-24,24,0),p("Fine","cent",-100,100,0),p("Smoothing","ms",5,100,20),p("Spread","cent",0,30,0)});
        add("night-scope","Night Scope","Meter","Spectrum, peak, RMS and stereo correlation",{p("Display range","dB",36,96,72),p("Peak hold","s",.1,5,1,true)});
        add("crown-master","Crown Master","Mastering","Switchable EQ, compression, saturation and stereo stages",{p("EQ enabled","",0,1,1,false,true),p("Low gain","dB",-6,6,0),p("High gain","dB",-6,6,0),p("Compression enabled","",0,1,1,false,true),p("Threshold","dB",-36,0,-12),p("Ratio","",1,6,1.5,true),p("Saturation enabled","",0,1,0,false,true),p("Drive","dB",0,12,1),p("Stereo enabled","",0,1,1,false,true),p("Width","%",0,150,100)});
        add("eclipse-limiter","Eclipse Limiter","Mastering","5 ms lookahead, 4x peak estimation, conservative 1.2 dB guard and K-weighted loudness",{p("Input","dB",-12,24,0),p("Ceiling","dBTP",-12,-.1,-1),p("Release","ms",20,1000,150,true),p("Stereo link","%",0,100,100)});
        s[31].parameters.resize(4); // The mastering limiter always enforces its ceiling; no parallel bypass or post gain.
        add("grave-dc","Grave DC","Restoration","DC offset removal with a gentle subsonic high-pass",{p("Cutoff","Hz",1,80,10,true)});
        add("hex-de-hum","Hex De-Hum","Restoration","Six narrow mains-frequency harmonic notches",{p("Mains","Hz",50,60,50,false,true),p("Harmonics","",1,6,4,false,true),p("Q","",5,20,20,true),p("Depth","%",0,100,100)});
        add("dusk-tilt","Dusk Tilt EQ","EQ","Complementary low and high shelves around a pivot",{p("Pivot","Hz",150,6000,1000,true),p("Tilt","dB",-12,12,2)});
        add("halo-exciter","Halo Exciter","Distortion","High-band harmonic excitation with DC rejection",{p("Crossover","Hz",1200,12000,4000,true),p("Drive","dB",0,24,6),p("Amount","%",0,100,20)});
        add("abyss-sub","Abyss Sub","Bass","Filtered octave divider for monophonic bass; harmonic synthesis",{p("Tracking low-pass","Hz",40,400,140,true),p("Sub level","dB",-48,0,-18),p("Sub low-pass","Hz",30,200,90,true)});
        add("silk-de-plosive","Silk De-Plosive","Vocal","Dynamic low-band reduction for plosives",{p("Frequency","Hz",60,350,160,true),p("Threshold","dB",-60,0,-24),p("Range","dB",-24,0,-10),p("Release","ms",20,500,100,true)});
        add("whisper-breath","Whisper Breath","Vocal","Reduce quiet high-band breaths while preserving louder words",{p("Crossover","Hz",1500,9000,3500,true),p("Threshold","dB",-70,-12,-36),p("Range","dB",-24,0,-8),p("Release","ms",10,400,90,true)});
        add("velvet-rider","Velvet Rider","Vocal","Bounded RMS level riding with a silence gate",{p("Target RMS","dBFS",-36,-6,-20),p("Range","dB",0,12,6),p("Response","ms",50,2000,400,true),p("Silence floor","dBFS",-80,-30,-55)});
        add("time-align","Time Align","Utility","Independent channel delays with polarity switches",{p("Left delay","ms",0,100,0),p("Right delay","ms",0,100,5),p("Invert left","",0,1,0,false,true),p("Invert right","",0,1,0,false,true)});
        auto duck=delay;duck.push_back(p("Duck threshold","dB",-60,0,-28));duck.push_back(p("Duck depth","dB",0,36,12));duck.push_back(p("Recovery","ms",20,1500,250,true));add("echo-duck","Echo Duck","Delay","Input-controlled ducking echo with tempo sync",duck);
        add("prism-resonator","Prism Resonator","Filter","Three tuned resonant bands with harmonic spacing",{p("Fundamental","Hz",40,2000,220,true),p("Q","",1,20,8,true),p("Harmonic spacing","",1,3,2),p("Amount","%",0,100,35)});
        add("ritual-ring","Ritual Ring","Modulation","Stereo ring modulation with controllable carrier offset",{p("Carrier","Hz",.1,5000,110,true),p("Stereo phase","deg",0,180,90),p("Depth","%",0,100,100)});
        add("shadow-crossfeed","Shadow Crossfeed","Stereo","Low-pass crossfeed for headphone checking",{p("Crossfeed","%",0,50,18),p("Low-pass","Hz",300,5000,1200,true)});
        add("scar-parallel","Scar Parallel","Dynamics","Upward compression below threshold with a noise floor",{p("Threshold","dB",-48,-6,-24),p("Ratio","",1,8,2,true),p("Maximum lift","dB",0,18,6),p("Response","ms",5,500,60,true),p("Noise floor","dB",-90,-36,-65)});
        s.push_back({"oracle-assist","Oracle Assist","Assistant","Local sound analysis and 30 scenario presets; proposes a real Vamp effect chain",{p("Scenario","",0,29,0,false,true),p("Strength","%",0,100,50),p("Listen duration","s",1,30,4,false,true)}});
        for(int i:{16,17,18,19,20,21,22,23})s[i].parameters[s[i].parameters.size()-2].initial=i>=18&&i<=20?20:35;
        s[41].parameters[s[41].parameters.size()-2].initial=25;
        return s;
    }();return result;
}
int nativeEffectIndex(const fs::path&path){if(path.extension()!=L".vampfx")return -1;auto id=utf8(path.stem().wstring());for(int i=0;i<(int)nativeSpecs().size();i++)if(nativeSpecs()[i].id==id)return i;return -1;}
std::vector<Json>nativePluginCatalog(){std::vector<Json>result;for(auto&s:nativeSpecs())result.push_back({{"path",utf8((fs::path(L"@vamp")/(wide(s.id)+L".vampfx")).wstring())},{"name",s.name},{"vendor","Karsei"},{"instrument",false},{"category",s.category},{"ok",true},{"vamp",true}});return result;}
}
