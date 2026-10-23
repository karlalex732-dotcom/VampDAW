#pragma once
#include "plugin.hpp"
#include "capture.hpp"
#include "asio.hpp"
#include "dsp_meter.hpp"
#include "dsp_workers.hpp"
#include "audio_recall.hpp"
#include <thread>
#include <mmsystem.h>

namespace vamp {
struct Event {double beat=0;int track=0,pitch=60,id=0;float velocity=.75f;bool on=true;double sampleOffset=0,duration=0,fadeIn=0,fadeOut=0;float clipGain=1;double elapsed=0;float pan=0,fine=0;int channel=0;const Sample*audio=nullptr;uint64_t signature=0;};
using PluginRack=std::array<std::vector<std::shared_ptr<Plugin>>,TrackCount+1>;
struct StereoDelay{
    std::vector<float>left,right;size_t head=0;int samples=0;
    explicit StereoDelay(int n):left(n?n+1:0),right(n?n+1:0),samples(n){}
    void clear(){std::fill(left.begin(),left.end(),0);std::fill(right.begin(),right.end(),0);head=0;}
    void process(float&l,float&r){if(!samples)return;size_t tail=(head+1)%left.size();float a=left[tail],b=right[tail];left[head]=l;right[head]=r;head=tail;l=a;r=b;}
};
struct TrackDSP{
    std::array<float,MaxBlock>sourceL{},sourceR{},left{},right{},sideL{},sideR{},postL{},postR{};
    std::vector<PluginNote>notes;int noteCount=0;
    explicit TrackDSP(size_t events=0):notes(events){}
};
struct RenderEdge{int target=MasterChannel;float gain=1;bool sidechain=false,preFader=false;std::shared_ptr<StereoDelay>delay;};
struct RenderTrack {
    float volume=.65f,pan=0,gainL=1,gainR=1;bool audible=true,external=false,autoEnabled=false;int sound=0,midiChannel=1,delay=0;
    std::vector<Point> automation;std::shared_ptr<Sample> sample;bool sampler=false,chromatic=false,oneShot=true;int rootNote=36;std::vector<Slice>slices;std::vector<std::shared_ptr<Plugin>> plugins;
    int autoTarget=0,autoSlot=-1;uint32_t autoParam=0;
    bool volumeAutomation=false,panAutomation=false;std::vector<Point>volumeEnvelope,panEnvelope;std::array<std::vector<AutomationLane>,PluginSlotCount>pluginEnvelopes;
    std::vector<uint64_t>eventSignatures;std::vector<bool>bypass;std::vector<RenderEdge>edges;std::shared_ptr<TrackDSP>dsp;std::shared_ptr<StereoDelay>sourceDelay;
    int inputLatency=0,outputLatency=0,pluginLatency=0,inputPair=0,inputMode=0,asioInputPair=0;bool scheduled=false,monitor=false,audioInput=false,frozen=false,armed=false;std::vector<std::shared_ptr<Sample>>clipSamples;
};
struct RenderPlan {TempoMap clock;std::vector<std::vector<int>>levels;bool parallelSafe=true;
    double bpm=112,length=32,maxDuration=0,loopStart=0,loopEnd=32;uint64_t contentSignature=0;bool loop=true;std::vector<RenderTrack>tracks;RenderTrack master;std::vector<int>order;std::vector<Event>events;int latency=0,tapTrack=-1;};
std::unique_ptr<RenderPlan>compile(const Project&p,const PluginRack&plugins={},const RenderPlan*previous=nullptr);
struct MidiCommand {uint32_t message=0;int track=0;};
class Renderer {
    DspWorkers dspWorkers;
    void renderSlice(const RenderPlan&,float*,int,bool,const float*,const float*,const float*const*,const float*const*,int);
    struct Voice{uint64_t signature=0;bool active=false,releasing=false,sustainPending=false,instrumentNote=false;int track=0,pitch=60,id=0,channel=0;double phase=0,step=0,position=0,audioAge=0,duration=0,fadeIn=0,fadeOut=0;float gain=0,env=0,clipGain=1,noteL=1,noteR=1;double sampleBegin=0,sampleEnd=0;bool reverse=false;uint32_t noise=1;const Sample*audio=nullptr;};
    std::array<Voice,MaxVoices> voices{};std::array<std::array<float,2048>,3> waves{};
    std::array<std::array<bool,16>,TrackCount>sustain{};std::array<std::array<double,16>,TrackCount>bend{};
    int voiceCount=0,blockFrames=0;std::array<bool,TrackCount>active{};
    const RenderPlan*lastPlan=nullptr;size_t eventIndex=0;bool wasPlaying=false;uint64_t lastContent=~uint64_t(0);bool chaseRequested=true;
    bool resetVoices=false;std::array<MidiCommand,1024> pendingMidi{};int pendingCount=0;
    void event(const RenderPlan&p,const Event&e,int offset);
    void activate(const RenderPlan&p,int track);
public:
    double rate=48000,position=0;HMIDIOUT midiOut=nullptr;
    int metronomeSound=0;uint32_t clickNoise=0x762439u;bool clickAccent=false;bool metronome=false;double prerollBeats=0,prerollTotal=0;int clickBeat=-1;double clickAge=1,clickFrequency=1000;
    std::array<float,MaxBlock>clicks{};
    void countIn(double beats){prerollBeats=prerollTotal=std::max(0.,beats);clickBeat=-1;clickAge=1;}
    std::array<float,TrackCount> peaks{};float masterPeak=0;bool finished=false;
    Renderer();void reset();void configureWorkers(int total){dspWorkers.configure(total);}int workers()const{return dspWorkers.size();}void locate(double frame){position=frame;lastPlan=nullptr;resetVoices=true;chaseRequested=true;finished=false;}void killNotes(){resetVoices=true;}void live(const RenderPlan&p,const MidiCommand&m,int offset=0);
    void render(const RenderPlan&p,float*interleaved,int frames,bool playing,const float*inputL=nullptr,const float*inputR=nullptr,const float*const*extraInputs=nullptr,const float*const*deviceInputs=nullptr,int deviceInputCount=0);
};
struct AudioPort{std::wstring id,name;};
struct MidiPort{UINT id;std::wstring name,uid;};
std::vector<AudioPort> audioPorts(bool input=false);std::vector<MidiPort> midiInputs();std::vector<MidiPort> midiOutputs();
std::wstring defaultAudioId(bool input=false);
class Engine {
    DspMeter dspMeter;
    std::thread worker;HANDLE stopEvent=nullptr;std::atomic<bool>workerFinished{true};std::atomic<const RenderPlan*> plan{nullptr};std::atomic<uint64_t> epoch{0};
    struct Retired{const RenderPlan*plan;uint64_t epoch;};std::vector<Retired> retired;
    std::wstring deviceId;void renderBlock(const RenderPlan&p,float*out,int frames,const float*l,const float*r,const float*const*deviceInputs=nullptr,int inputCount=0);void run();void runAsio();static void asioCallback(void*context,const float*inputL,const float*inputR,float*output,int frames,const float*const*inputs,int inputCount);
    std::atomic<const RenderPlan*>previewPlan{nullptr};std::atomic<bool>previewRestart{false},previewPlaying{false};Renderer previewRenderer;std::array<float,MaxBlock*2>previewBuffer{};
public:
    Renderer renderer;AudioCapture capture;std::array<AudioCapture,3>extraCapture;std::array<AggregateOutput,3>aggregateOutput;
    struct MultiRecording {int track=0,pair=0,mode=0,asio=0;fs::path path;std::unique_ptr<AudioCapture>capture=std::make_unique<AudioCapture>();};
    std::vector<std::unique_ptr<MultiRecording>>multiRecorders;AudioRecall recall;
    AudioCapture playlistRecorder;std::atomic<bool>recordInput{false};std::atomic<int>recordPair{0},recordInputMode{0};
    bool aggregateEnabled=false;std::array<std::wstring,3>aggregateOutputIds{},aggregateInputIds{};std::array<int,3>aggregateSources{MasterChannel,MasterChannel,MasterChannel};
    std::array<std::array<float,MaxBlock>,6>aggregateInputBuffer{};std::array<float,MaxBlock*2>aggregateSendBuffer{};Mpsc<MidiCommand,1024> midiQueue;std::atomic<bool> playing{false},rewind{false},midiPanic{false};
    std::atomic<int>metronomeSound{0};std::atomic<bool>metronome{false};std::atomic<double>countInRequest{0};std::atomic<double>countInRemaining{0};
    std::atomic<int> state{0};std::atomic<double> beat{0},cpu{0},cpuPeak{0},dspBlockMs{0},rate{48000},deviceRate{48000},latencyMs{0},seek{-1};
    std::atomic<int>actualBuffer{0},callbackFrames{0};std::atomic<uint64_t>processedFrames{0},callbackCount{0};std::atomic<double>dspSeconds{0};std::atomic<bool>wasapiResampling{false},asioConfigurationChanged{false};
    std::atomic<unsigned> underruns{0};std::array<std::atomic<float>,TrackCount> peaks{};std::atomic<float> masterPeak{0};
    std::atomic<HMIDIOUT> output{nullptr};
    std::atomic<HWND>hostWindow{nullptr};std::atomic<int>requestedBuffer{0},asioInput{0},asioOutput{0},asioInputs{0},asioOutputs{0};std::atomic<double>requestedRate{0},inputLatencyMs{0};std::atomic<bool>requestAsioPanel{false};std::wstring audioError;
    Engine();~Engine();void publish(std::unique_ptr<RenderPlan>p);void start(const std::wstring&id=L"");void stop();void panic();
    void measureDsp(int frames,double seconds);
    void recordAudioBlock(const float*l,const float*r,const float*const*extra,int frames,double bpm=120,const float*const*deviceInputs=nullptr,int inputCount=0);
    void audition(std::unique_ptr<RenderPlan>p);void stopAudition(){previewPlaying=false;}bool auditioning()const{return previewPlaying.load();}
    const RenderPlan*uiSnapshot()const{return plan.load();}
};
void exportWav(const RenderPlan&p,const fs::path&path,int sampleRate=48000);
enum class AudioFormat{Wav,Mp3,Flac,Ogg,Aac,Opus,Aiff};
struct ExportOptions{int rate=48000,bits=24,channels=2,bitrate=192,quality=6;AudioFormat format=AudioFormat::Wav;double startBeat=0,endBeat=0,tailSeconds=0;bool normalize=false,dither=false;};
struct ExportControl{std::atomic<double>progress{0};std::atomic<bool>cancel{false};std::atomic<int>phase{0};double renderWeight=1;};
void renderWav(const RenderPlan&p,const fs::path&path,const ExportOptions&options,ExportControl*control=nullptr);
const wchar_t*audioExtension(AudioFormat format);const wchar_t*audioFormatName(AudioFormat format);
fs::path audioEncoderPath();
void renderAudio(const RenderPlan&p,const fs::path&path,const ExportOptions&options,ExportControl*control=nullptr);
}
