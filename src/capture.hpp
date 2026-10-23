#pragma once
#include "model.hpp"
#include "queue.hpp"
#include "aggregate.hpp"
#include <thread>
namespace vamp {
class WaveWriter {
    std::ofstream file;uint64_t count=0,lastHeader=0;int rate,channels;bool finished=false;
    void header(uint32_t size);
public:
    WaveWriter(const fs::path&path,int sampleRate,int channels=2);~WaveWriter();
    void write(const float*left,const float*right,int frames);void finish();
    uint64_t frames()const{return count;}
};
class AudioCapture {
    struct Stereo{float left=0,right=0;};
    struct Packet{int frames=0;uint64_t startFrame=0;std::array<float,MaxBlock>left{},right{};};
    AsyncAudioBuffer monitor;std::array<float,MaxBlock*2>monitorBuffer{};Spsc<Packet,64>diskQueue;
    std::thread worker,diskWorker;HANDLE stopEvent;std::wstring device;double sampleRate=48000;
    std::unique_ptr<WaveWriter>writer;std::atomic<bool>diskStop{false};std::atomic<uint64_t>capturedFrames{0};void run();void diskRun();
public:
    std::atomic<int>state{0};std::atomic<float>peak{0};std::atomic<double>latencyMs{0};std::atomic<uint64_t>recordedFrames{0};std::atomic<unsigned>discontinuities{0},droppedPackets{0};
    std::array<std::atomic<float>,512>peakHistory{};std::array<std::atomic<uint64_t>,512>peakHistoryFrame{};std::atomic<uint64_t>peakHistoryCount{0};
    AudioCapture();~AudioCapture();void start(const std::wstring&id,double rate,const fs::path&recordPath={});void stop();
    void startExternal(double rate,const fs::path&recordPath={},int channels=2);void ingest(const float*left,const float*right,int frames);
    void read(float*left,float*right,int frames);
};
}
