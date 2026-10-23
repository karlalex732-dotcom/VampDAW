#include "editing.hpp"
static void playlistRecordingTests(){
    auto engine=std::make_unique<Engine>();std::array<float,256>left{},right{};left.fill(.2f);right.fill(-.6f);const float*extra[6]{};extra[2]=left.data();extra[3]=right.data();
    auto mono=fs::absolute(L"work/test-results/input-mono.wav"),stereo=fs::absolute(L"work/test-results/input-stereo.wav");
    engine->playlistRecorder.startExternal(48000,mono,1);engine->recordInput=true;engine->recordPair=2;engine->recordInputMode=2;
    engine->recordAudioBlock(nullptr,nullptr,extra,256); // stopped transport must not consume time
    engine->playing=true;for(int i=0;i<8;i++)engine->recordAudioBlock(nullptr,nullptr,extra,256);
    engine->recordInput=false;engine->recordAudioBlock(nullptr,nullptr,extra,256);engine->playlistRecorder.stop();
    auto a=readWav(mono);check(a->left.size()==2048&&std::abs(a->left[700]+.6f)<1e-5&&a->left==a->right,"mono records only the chosen input; stopped and disarmed blocks excluded");
    {std::ifstream f(mono,std::ios::binary);f.seekg(22);uint16_t channels=0;f.read((char*)&channels,2);check(channels==1,"mono takes use a true mono WAV header");}
    engine->playlistRecorder.startExternal(48000,stereo,2);engine->recordPair=0;engine->recordInputMode=0;engine->recordInput=true;
    for(int i=0;i<8;i++)engine->recordAudioBlock(left.data(),right.data(),extra,256);engine->recordInput=false;engine->playlistRecorder.stop();auto b=readWav(stereo);
    check(b->left.size()==2048&&std::abs(b->left[700]-.2f)<1e-5&&std::abs(b->right[700]+.6f)<1e-5,"stereo take preserves separate inputs");
    check(engine->playlistRecorder.peakHistoryCount==8&&engine->playlistRecorder.peakHistoryFrame[7]==2048,"live waveform stores capture time");
    for(int bars:{1,2}){
        Project empty;empty.bpm=120;empty.tracks.resize(1);auto countPlan=compile(empty);engine->renderer.reset();engine->renderer.rate=48000;engine->renderer.countIn(bars*4);
        auto file=fs::absolute(L"work/test-results/count-in-"+std::to_wstring(bars)+L".wav");engine->playlistRecorder.startExternal(48000,file);engine->recordInput=true;engine->playing=true;int remaining=bars*96000+1024;std::array<float,512>output{};
        while(remaining){int n=std::min(remaining,256);engine->recordAudioBlock(left.data(),right.data(),nullptr,n,120);engine->renderer.render(*countPlan,output.data(),n,true);remaining-=n;}
        engine->recordInput=false;engine->playlistRecorder.stop();auto countTake=readWav(file);check(countTake->left.size()==1024&&engine->renderer.position==1024,"one and two bar count-ins record only audio after the exact count boundary");
    }
    Project p;p.tracks.resize(1);p.bpm=120;p.loop=false;auto&t=p.tracks[0];t.audioInput=true;t.name="Guitar";t.inputMode=2;t.inputDevice="test-device";t.asioInputPair=4;t.notes={{0,1,60,1}};
    Clip first;first.beat=1;first.length=.08;first.audio=a;first.name="Take 1";Clip second=first;second.beat=2;second.audio=b;second.name="Take 2";t.clips={first,second};
    auto plan=compile(p);check(plan->events.size()==4&&plan->events[0].audio==a.get()&&plan->events[2].audio==b.get(),"multiple takes schedule their own audio on the same channel");
    auto renderer=std::make_unique<Renderer>();std::array<float,512>out{};renderer->locate(24000);renderer->render(*plan,out.data(),256,true);check(out[500]<0&&out[501]<0,"first take plays its mono asset");renderer->locate(48000);renderer->render(*plan,out.data(),256,true);check(out[500]>0&&out[501]<0,"second take plays its own stereo asset");
    auto path=fs::absolute(L"work/test-results/audio-input.vamp");saveProject(p,path);auto restored=loadProject(path);auto&rt=restored.tracks[0];check(rt.name=="Guitar"&&rt.audioInput&&rt.inputDevice=="test-device"&&rt.asioInputPair==4&&rt.inputMode==2,"per-track audio input persists");check(rt.clips.size()==2&&rt.clips[0].audio&&rt.clips[1].audio&&rt.clips[0].audio->path!=rt.clips[1].audio->path,"multiple take references survive project reload");
    auto original=rt.clips[0].audio;auto copied=transformClips(restored,{{0,0}},3,1,true);check(restored.tracks[1].clips[0].audio==original,"recorded take drags to another track with its asset");
    splitArrangementAt(p,1.04);check(p.tracks[0].clips.size()==3&&std::abs(p.tracks[0].clips[1].offset-.02)<1e-6,"splitting a recorded take advances its source offset");
    Project inputOnly;inputOnly.tracks.resize(1);inputOnly.tracks[0].audioInput=true;inputOnly.tracks[0].notes={{0,1,60,1}};inputOnly.tracks[0].clips={{0,4}};check(compile(inputOnly)->events.empty(),"Audio Input never plays the built-in synth from hidden notes");
    inputOnly.tracks[0].monitor=true;inputOnly.tracks[0].inputMode=2;auto monitor=compile(inputOnly);renderer->reset();renderer->render(*monitor,out.data(),256,false,left.data(),right.data());check(out[500]<0&&std::abs(out[500]-out[501])<1e-6,"mono monitor follows the selected right input");
}
