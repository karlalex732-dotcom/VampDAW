#include <set>
static void assistantTests(){
    constexpr double pi=3.14159265358979323846;constexpr int rate=48000;
    auto process=[](NativeEffect&fx,std::vector<float>&a,std::vector<float>&b){for(size_t at=0;at<a.size();at+=256)fx.process(a.data()+at,b.data()+at,(int)std::min<size_t>(256,a.size()-at),0,120,true,nullptr,0,nullptr,nullptr,nullptr,0,0);};
    auto set=[](NativeEffect&fx,int id,double real){fx.set(id,fx.spec().parameters[id].normalized(real));};
    auto power=[](const std::vector<float>&a,size_t start=24000){double sum=0;for(size_t i=start;i<a.size();i++)sum+=a[i]*a[i];return sum/std::max<size_t>(1,a.size()-start);};
    auto tone=[&](double hz,double gain=.1){std::vector<float>a(rate);for(int i=0;i<rate;i++)a[i]=(float)(gain*std::sin(2*pi*hz*i/rate));return a;};
    {
        NativeEffect fx(32,rate);auto a=tone(500);for(auto&v:a)v+=.2f;auto b=a;process(fx,a,b);double mean=std::accumulate(a.begin()+24000,a.end(),0.)/24000;check(std::abs(mean)<.0001,"Grave DC removes DC without losing AC");check(power(a)>.004,"DC blocker retains useful signal");
    }
    {
        NativeEffect fx(33,rate);auto a=tone(50),b=a;process(fx,a,b);check(power(a)<1e-6,"Hex De-Hum rejects the selected mains frequency");NativeEffect retain(33,rate);a=tone(1000);b=a;process(retain,a,b);check(power(a)>.004,"De-Hum preserves frequencies outside narrow notches");
    }
    {
        NativeEffect fx(34,rate);set(fx,1,10);fx.prepare(rate);auto a=tone(100),b=a;process(fx,a,b);double low=power(a);fx.prepare(rate);a=tone(10000);b=a;process(fx,a,b);check(power(a)>low*5,"Tilt EQ raises highs relative to lows");
    }
    {
        NativeEffect fx(40,rate);set(fx,0,10);set(fx,1,20);fx.prepare(rate);std::vector<float>a(2400),b(2400);a[0]=b[0]=1;process(fx,a,b);check(std::abs(a[480]-1)<1e-6&&std::abs(b[960]-1)<1e-6&&a[0]==0&&b[0]==0,"Time Align delays channels independently with sample accuracy");
    }
    {
        NativeEffect fx(37,rate);set(fx,1,-40);fx.prepare(rate);auto a=tone(80,.5),b=a;process(fx,a,b);check(power(a)<.06,"De-Plosive attenuates a loud low-frequency burst");
        NativeEffect breath(38,rate);a=tone(8000,.001);b=a;process(breath,a,b);check(power(a)<2e-7,"Whisper Breath reduces quiet high-band energy");
        NativeEffect rider(39,rate);set(rider,2,50);rider.prepare(rate);a.assign(rate,.3f);b=a;process(rider,a,b);check(a.back()<.2&&a.back()>.14,"Velvet Rider lowers a sustained signal within its gain range");
    }
    {
        NativeEffect cross(44,rate);auto a=tone(300),b=std::vector<float>(rate);process(cross,a,b);check(power(b)>1e-5,"Crossfeed routes filtered audio to the opposite channel");
        NativeEffect lift(45,rate);auto c=tone(500,.02),d=c;double before=power(c);process(lift,c,d);check(power(c)>before*1.4,"Scar Parallel lifts quiet useful signal");
    }
    std::vector<float>a(rate*4),b(a.size());for(size_t i=0;i<a.size();i++){a[i]=(float)(.2*std::sin(2*pi*80*i/rate));b[i]=-a[i];}
    auto analysis=analyzeAssistant(a.data(),b.data(),a.size(),rate);
    check(analysis.valid&&analysis.bass>.9&&analysis.correlation<-.99,"Assistant measures bass and anti-phase stereo without mono cancellation");
    check(std::abs(analysis.rms+16.9897)<.05&&std::abs(analysis.crest-3.0103)<.05,"Assistant RMS and crest calibrate against a known sine");
    check(detectAssistantPreset(analysis,false)==12&&detectAssistantPreset(analysis,true)==25,"Auto profile distinguishes bass and the master context");
    auto bright=a;for(size_t i=0;i<bright.size();i++)bright[i]=(float)(.1*std::sin(2*pi*9000*i/rate));auto high=analyzeAssistant(bright.data(),bright.data(),bright.size(),rate);check(high.air>.99&&detectAssistantPreset(high,false)==10,"Automatic selection responds to measured high-frequency content");
    std::vector<float>silence(rate);check(!analyzeAssistant(silence.data(),silence.data(),silence.size(),rate).valid,"Silence never creates an automatic analysis");
    NativeEffect oracle(46,rate);check(oracle.assistantCapture->request()&&!oracle.assistantCapture->request(),"Only one assistant capture may run at once");auto quiet=silence;process(oracle,silence,quiet);check(oracle.assistantCapture->progress==0,"Assistant waits for real audio before capturing");
    auto originalA=a,originalB=b;process(oracle,a,b);check(a==originalA&&b==originalB,"Oracle analysis is bit-identical passthrough");check(oracle.assistantCapture->state==3&&oracle.assistantCapture->progress==rate*4,"Four seconds are captured and release-published for UI-only analysis");
    Json captureDurations=Json::array();
    for(int captureRate:{44100,48000,96000}){
        AssistantCapture capture(captureRate);auto*storage=capture.left.data();std::array<float,512>signal;signal.fill(.1f);
        for(int seconds:{1,7,30}){
            check(capture.request(seconds)&&!capture.request(30),"A new duration cannot replace a running capture");size_t expected=(size_t)captureRate*seconds;
            check(capture.targetFrames==expected&&capture.progress==0,"Duration sets the exact number of frames and resets progress");
            while(capture.state!=3)capture.process(signal.data(),signal.data(),512);
            check(capture.progress==expected&&capture.written==expected&&capture.left.data()==storage,"Capture ends exactly at 1–30 seconds without audio-thread allocation");
            auto measured=analyzeAssistant(capture.left.data(),capture.right.data(),expected,captureRate);check(measured.valid&&std::abs(measured.seconds-seconds)<1e-10,"Analysis measures only the chosen duration, not buffer capacity");
            captureDurations.push_back({{"rate",captureRate},{"seconds",seconds},{"frames",expected}});capture.state=0;
        }
        check(capture.request(-10)&&capture.targetFrames==(size_t)captureRate,"Capture clamps below one second");capture.state=0;
        check(capture.request(80)&&capture.targetFrames==(size_t)captureRate*30,"Capture clamps above thirty seconds");
    }
    check(assistantPresets().size()==30,"Exactly 30 situational presets");std::set<std::string>names;Json presets=Json::array();
    analysis.dc=.01;
    for(int scenario=0;scenario<30;scenario++){
        check(names.insert(assistantPresets()[scenario].name).second,"Assistant preset names are unique");auto plan=makeAssistantPlan(scenario,65,&analysis);check(plan.analyzed&&plan.steps.size()<=14&&!plan.steps.empty(),"Every analyzed preset fits a fresh channel with instrument and assistant");check(plan.steps.front().kind==32,"Measured DC offset adds a correction stage");
        PluginRack rack;Project project;project.tracks.resize(1);auto&track=project.tracks[0];Json steps=Json::array();
        for(auto&step:plan.steps){check(step.kind>=0&&step.kind<46,"Assistant recommends real processing effects only");auto effect=std::make_shared<Plugin>();std::string message;auto path=fs::path(L"@vamp")/(wide(nativeSpecs()[step.kind].id)+L".vampfx");check(effect->load(path,rate,message),"Each recommended effect loads");for(auto&setting:step.settings){check(setting.first>=0&&setting.first<(int)effect->native->spec().parameters.size(),"Assistant uses valid parameter IDs");auto&p=effect->native->spec().parameters[setting.first];check(setting.second>=p.low&&setting.second<=p.high,"Assistant real values remain in parameter bounds");effect->native->set(setting.first,p.normalized(setting.second));}effect->prepare(rate);track.pluginPaths.push_back(utf8(path.wstring()));track.pluginStates.push_back(effect->state());rack[0].push_back(effect);steps.push_back(nativeSpecs()[step.kind].name);}
        auto saved=projectFromJson(projectJson(project,L"work/test-results/oracle.vamp"),L"work/test-results/oracle.vamp");check(saved.tracks[0].pluginStates==track.pluginStates,"Generated chain persists actual settings");
        auto l=tone(1000),r=l;for(auto&effect:rack[0])for(size_t at=0;at<l.size();at+=256)effect->process(l.data()+at,r.data()+at,(int)std::min<size_t>(256,l.size()-at),nullptr,0,0,120,true);for(float v:l)check(std::isfinite(v)&&std::abs(v)<10,"Every generated chain produces bounded audio");
        if(assistantPresets()[scenario].mastering)check(plan.steps.back().kind==31,"Master presets finish with the real lookahead limiter");presets.push_back({{"name",assistantPresets()[scenario].name},{"chain",steps}});
    }
    oracle.assistantAuto=false;set(oracle,0,29);set(oracle,2,23);auto state=oracle.state();NativeEffect restored(46,rate);restored.restore(state);check(!restored.assistantAuto&&restored.values[0].load()==1&&std::abs(restored.spec().parameters[2].value(restored.values[2].load())-23)<1e-10,"Assistant scenario, listening duration and auto/manual mode persist");
    auto legacy=state;legacy["parameters"].erase(legacy["parameters"].end()-1);restored.restore(legacy);check(restored.spec().parameters[2].value(restored.values[2].load())==4,"Existing projects keep the four-second default");
    Json report={{"passed",true},{"presets",presets},{"capture_durations",captureDurations},{"duration_persistence",true},{"capture_passthrough",true},{"rms_dbfs",analysis.rms},{"anti_phase_correlation",analysis.correlation},{"spectral_auto_selection",true},{"neural_network",false}};std::ofstream(L"work/test-results/oracle-assist.json")<<report.dump(2);
}
