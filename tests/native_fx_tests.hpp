#include "native_fx.hpp"
#include <chrono>
static void nativeFxTests(){
    constexpr double pi=3.14159265358979323846;
    auto setReal=[](NativeEffect&fx,int id,double value){fx.set(id,fx.spec().parameters[id].normalized(value));};
    auto process=[](NativeEffect&fx,std::vector<float>&l,std::vector<float>&r,const float*side=nullptr){for(size_t at=0;at<l.size();at+=256){int n=(int)std::min<size_t>(256,l.size()-at);fx.process(l.data()+at,r.data()+at,n,at/fx.rate*2,120,true,nullptr,0,side?side+at:nullptr,side?side+at:nullptr,nullptr,0,0);}};
    auto energy=[](const std::vector<float>&v,size_t first=0){double sum=0;for(size_t i=first;i<v.size();i++)sum+=v[i]*v[i];return sum/std::max<size_t>(1,v.size()-first);};
    Json report={{"passed",false},{"effects",Json::array()}};
    check(nativeSpecs().size()==47,"32 original effects plus 14 effects and Oracle Assist");
    for(int kind=0;kind<(int)nativeSpecs().size();kind++){
        auto&spec=nativeSpecs()[kind];check(spec.parameters.size()<=NativeEffect::ParameterCapacity,"native effect parameter capacity");
        auto plug=std::make_shared<Plugin>();std::string error;
        check(plug->load(fs::path(L"@vamp")/(wide(spec.id)+L".vampfx"),48000,error),"native catalog loads through the normal host");
        check(!plug->instrument()&&plug->info.vendor=="Karsei","native effect metadata");
        double maximum=0,elapsedMs=0;
        for(double rate:{44100.,48000.,96000.}){
            NativeEffect fx(kind,rate);auto initial=fx.state();fx.set(0,.75);fx.restore(initial);check(fx.state()["parameters"]==initial["parameters"],"native preset restores all parameters");
            std::vector<float>left((size_t)rate),right(left.size());for(size_t n=0;n<left.size();n++){double hz=30+15000*n/rate;left[n]=(float)(.2*std::sin(2*pi*(30*n/rate+7500*n*n/(rate*rate))));right[n]=(float)(left[n]*.7+.1*std::sin(2*pi*119*n/rate));}
            auto start=std::chrono::steady_clock::now();process(fx,left,right);elapsedMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            for(auto*channel:{&left,&right})for(float value:*channel){check(std::isfinite(value)&&std::abs(value)<100,"native default DSP remains finite and bounded");maximum=std::max(maximum,double(std::abs(value)));}
            for(double extreme:{0.,1.}){for(size_t i=0;i<spec.parameters.size();i++)fx.set((uint32_t)i,extreme);for(int block=0;block<24;block++){std::array<float,256>a{},b{};for(int i=0;i<256;i++)a[i]=b[i]=(float)(.1*std::sin((block*256+i)*.13));fx.process(a.data(),b.data(),256,0,120,true,nullptr,0,nullptr,nullptr,nullptr,0,0);for(int i=0;i<256;i++)check(std::isfinite(a[i])&&std::isfinite(b[i])&&std::abs(a[i])<1000&&std::abs(b[i])<1000,"native parameter extremes remain stable");}}
        }
        report["effects"].push_back({{"name",spec.name},{"maximum",maximum},{"three_seconds_processing_ms",elapsedMs}});
    }
    {
        NativeEffect eq(0,48000);setReal(eq,0,1000);setReal(eq,1,6);eq.prepare(48000);
        std::vector<float>a(48000),b;for(size_t i=0;i<a.size();i++)a[i]=(float)(.1*std::sin(2*pi*1000*i/48000));b=a;process(eq,a,b);double db=10*std::log10(energy(a,24000)/.005);check(std::abs(db-6)<.05,"Blood EQ has the requested gain at the band centre");report["eq_measured_gain_db"]=db;
    }
    {
        Json responseReport=Json::array();double maximumError=0;
        for(int mode=0;mode<8;mode++)for(double hz:{200.,900.,3200.,12000.}){
            NativeEffect eq(0,48000);for(int band=0;band<7;band++)setReal(eq,23+band,7);
            setReal(eq,29,mode);setReal(eq,20,1800);setReal(eq,21,9);setReal(eq,22,1.2);
            setReal(eq,18,73);setReal(eq,19,-2);eq.prepare(48000);
            std::vector<float>a(48000),b;for(size_t i=0;i<a.size();i++)a[i]=(float)(.05*std::sin(2*pi*hz*i/48000));b=a;process(eq,a,b);
            double measured=10*std::log10(energy(a,24000)/.00125),drawn=eq.eqResponseDb(hz),error=std::abs(measured-drawn);maximumError=std::max(maximumError,error);
            check(error<.025,"seven-band EQ display matches measured audio for every filter type, wet mix and output gain");
            responseReport.push_back({{"mode",mode},{"hz",hz},{"audio_db",measured},{"display_db",drawn}});
        }
        report["eq_response_max_error_db"]=maximumError;report["eq_response_measurements"]=responseReport;
        NativeEffect original(0,48000);setReal(original,18,63);setReal(original,19,-4);setReal(original,4,5);
        auto legacy=original.state();legacy["version"]=1;legacy["parameters"].erase(legacy["parameters"].begin()+20,legacy["parameters"].end());
        for(auto key:{"bankA","bankB"})legacy[key].erase(legacy[key].begin()+24,legacy[key].end());
        NativeEffect restored(0,48000);restored.restore(legacy);
        for(int id=0;id<20;id++)check(restored.values[id].load()==original.values[id].load(),"legacy EQ parameter IDs, wet mix and output remain compatible");
        check(restored.bankA[21]==.5&&restored.bankB[21]==.5&&restored.bankA[29]==0,"legacy A/B banks keep the seventh EQ band neutral");
        check(restored.spec().parameters.size()==30&&restored.values[21].load()==.5&&restored.values[29].load()==0,"legacy EQ adds a neutral seventh band");
        setReal(restored,21,7);setReal(restored,29,2);auto saved=restored.state();NativeEffect roundtrip(0,48000);roundtrip.restore(saved);
        check(roundtrip.state()["parameters"]==saved["parameters"],"all seven bands and filter modes survive project save/load");
        NativeEffect quiet(0,48000),analyzed(0,48000);analyzed.analyzerRunning=true;
        std::vector<float>a(8192),b(a.size());for(size_t i=0;i<a.size();i++){a[i]=(float)(.1*std::sin(2*pi*750*i/48000));b[i]=-a[i];}
        auto c=a,d=b;process(quiet,a,b);process(analyzed,c,d);
        check(a==c&&b==d,"stereo spectrum capture never changes processed sound");
        check(quiet.eqAnalysis->head==0&&analyzed.eqAnalysis->head==8192,"spectrum capture runs only while the analyzer is visible and enabled");
        double power=0;for(int i=0;i<4096;i++){float left=analyzed.eqAnalysis->output[i].load(),right=analyzed.eqAnalysis->outputR[i].load();power+=left*left+right*right;check(std::abs(left+right)<1e-7,"spectrum retains both channels for opposite-phase stereo");}
        check(power>1,"opposite-phase stereo remains visible to the analyzer");
        analyzed.analyzerRunning=false;process(analyzed,c,d);check(analyzed.eqAnalysis->head==8192,"closing the EQ pauses spectrum capture");
    }
    {
        NativeEffect fx(3,48000);setReal(fx,0,-20);setReal(fx,1,4);setReal(fx,2,.1);fx.prepare(48000);std::vector<float>a(48000,.5f),b=a;process(fx,a,b);double expected=.5*std::pow(10.,(-20-20*std::log10(.5))*.75/20);check(std::abs(a.back()-expected)<.001,"compressor obeys threshold and ratio");
        setReal(fx,5,1);fx.prepare(48000);a.assign(48000,.1f);b=a;std::vector<float>side(48000,1.f);process(fx,a,b,side.data());check(a.back()<.025,"external sidechain drives gain reduction");
    }
    {
        NativeEffect fx(7,48000);setReal(fx,0,-40);setReal(fx,2,0);setReal(fx,3,5);fx.prepare(48000);std::vector<float>a(48000,.001f),b=a;process(fx,a,b);check(a.back()<.000001,"gate closes below threshold");
    }
    {
        NativeEffect fx(16,48000);setReal(fx,0,100);setReal(fx,1,0);setReal(fx,3,0);setReal(fx,5,100);fx.prepare(48000);std::vector<float>a(9600),b;a[0]=1;b=a;process(fx,a,b);check(std::abs(a[4800]-1)<1e-5&&energy(a,4801)<1e-12,"echo has exact delay and no feedback when disabled");
    }
    {
        NativeEffect fx(20,48000);setReal(fx,5,100);fx.prepare(48000);std::vector<float>a(96000),b;a[0]=1;b=a;process(fx,a,b);check(energy(a,24000)>1e-12,"hall generates a decaying tail");
    }
    {
        NativeEffect fx(28,48000);setReal(fx,0,12);fx.prepare(48000);std::vector<float>a(96000),b;for(size_t i=0;i<a.size();i++)a[i]=(float)(.2*std::sin(2*pi*220*i/48000));b=a;process(fx,a,b);double best=0;int frequency=0;for(int hz=400;hz<=480;hz++){double re=0,im=0;for(size_t n=48000;n<96000;n++){re+=a[n]*std::cos(2*pi*hz*n/48000);im+=a[n]*std::sin(2*pi*hz*n/48000);}double magnitude=re*re+im*im;if(magnitude>best){best=magnitude;frequency=hz;}}check(std::abs(frequency-440)<=1,"pitch shifter raises a steady tone by an octave");report["pitch_measured_hz"]=frequency;
    }
    {
        NativeEffect fx(27,48000);setReal(fx,0,-6);fx.prepare(48000);std::vector<float>a(48000,.5f),b=a;process(fx,a,b);check(std::abs(a.back()-.5*std::pow(10.,-.3))<1e-6,"utility gain is precise");
        std::vector<Point>curve{{0,0},{2,1}};std::array<float,256>l{},r{};l.fill(.1f);r=l;fx.process(l.data(),r.data(),256,2,120,true,&curve,0,nullptr,nullptr,nullptr,0,0);check(l.back()>l.front(),"native parameters follow drawn automation");
    }
    {
        NativeEffect fx(29,48000);std::vector<float>a(480000),b;for(size_t n=0;n<a.size();n++)a[n]=(float)(.1*std::sin(2*pi*1000*n/48000));b=a;process(fx,a,b);check(std::abs(fx.integrated.load()+20)<.5,"K weighted stereo 1 kHz calibration is close to -20 LUFS");report["scope_1khz_stereo_lufs"]=fx.integrated.load();
        WaveWriter wav(L"work/test-results/native-meter.wav",48000);wav.write(a.data(),b.data(),(int)a.size());wav.finish();
    }
    {
        NativeEffect fx(31,48000);setReal(fx,0,12);fx.prepare(48000);std::vector<float>a(240000),b;uint32_t seed=777;for(size_t n=0;n<a.size();n++){seed=seed*1664525+1013904223;a[n]=(float)(.7*std::sin(2*pi*997*n/48000)+.2*(int32_t(seed)/2147483648.));}b=a;process(fx,a,b);double maximum=0;for(float value:a)maximum=std::max(maximum,double(std::abs(value)));check(maximum<std::pow(10.,-1./20)+1e-6,"limiter enforces its sample ceiling");WaveWriter wav(L"work/test-results/native-limiter.wav",48000);wav.write(a.data(),b.data(),(int)a.size());wav.finish();report["limiter_sample_peak_dbfs"]=20*std::log10(maximum);
    }
    report["passed"]=true;std::ofstream(L"work/test-results/native-effects.json")<<report.dump(2);
}
