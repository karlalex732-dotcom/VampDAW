#include "vst_lifecycle.hpp"
static void freezeTests(){
    check(processingNotificationAccepted(Steinberg::kResultFalse)&&processingNotificationAccepted(Steinberg::kNotImplemented),"VST3 processing notifications accept non-fatal status");
    check(!processingNotificationAccepted(Steinberg::kInternalError)&&!processingNotificationAccepted(Steinberg::kInvalidArgument),"VST3 processing notifications preserve real failures");
    Project p=demoProject();p.tracks.resize(1);p.bars=1;p.loop=false;auto&t=p.tracks[0];t.volume=.37f;t.pan=-.2f;t.automationEnabled=true;t.automationTarget=0;t.automation={{0,.2f},{4,.9f}};
    auto original=compile(p);auto tapped=compile(p);tapped->tapTrack=0;tapped->latency=tapped->tracks[0].outputLatency;
    ExportOptions options;options.bits=32;fs::path path=L"work/test-results/freeze-cache.wav";renderWav(*tapped,path,options);
    auto cache=readWav(path);check(cache->left.size()==(size_t)(4*60/p.bpm*48000+1),"freeze cache has the full song timeline");
    t.frozen=true;t.freezeBpm=p.bpm;t.freezeAudio=cache;auto frozen=compile(p);check(frozen->tracks[0].plugins.empty()&&!frozen->tracks[0].monitor,"frozen processing excludes instruments, FX and live monitoring");
    auto a=std::make_unique<Renderer>(),b=std::make_unique<Renderer>();std::array<float,1024>x{},y{};double error=0,energy=0;for(int block=0;block<160;block++){a->render(*original,x.data(),512,true);b->render(*frozen,y.data(),512,true);for(size_t i=0;i<x.size();i++){error=std::max(error,double(std::abs(x[i]-y[i])));energy+=y[i]*y[i];}}check(energy>0&&error<2e-6,"freeze preserves rendered source, volume, pan and automation");
    a->locate(1.7*60/p.bpm*48000);b->locate(1.7*60/p.bpm*48000);a->render(*original,x.data(),512,true);b->render(*frozen,y.data(),512,true);
    double seekError=0;for(size_t i=0;i<x.size();i++)seekError=std::max(seekError,double(std::abs(x[i]-y[i])));
    saveProject(p,L"work/test-results/frozen.vamp");auto restored=loadProject(L"work/test-results/frozen.vamp");check(restored.tracks[0].frozen&&restored.tracks[0].freezeAudio&&restored.tracks[0].notes.size()==t.notes.size(),"freeze and editable source survive project persistence");
    p.tracks.push_back(Track{});p.tracks[1].output=0;p.tracks[1].sends.push_back({0,1,true,false});auto routing=compile(p);check(routing->tracks[1].edges.empty(),"cached bus input and sidechain are not mixed twice");
    bool rejected=false;p.bpm+=1;try{compile(p);}catch(...){rejected=true;}check(rejected,"tempo changes require unfreezing");
    auto json=projectJson(restored,L"work/test-results/missing-freeze.vamp");json["tracks"][0]["freeze"]["audio"]="does-not-exist.wav";auto missing=projectFromJson(json,L"work/test-results/missing-freeze.vamp");check(!missing.tracks[0].frozen&&missing.tracks[0].notes.size()==t.notes.size(),"missing freeze cache restores editable source instead of losing the track");
    Json report={{"passed",true},{"max_audio_difference",error},{"seek_difference",seekError},{"source_preserved",true},{"bus_input_not_doubled",true}};std::ofstream(L"work/test-results/freeze.json")<<report.dump(2);
}
