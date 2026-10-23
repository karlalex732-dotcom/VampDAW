#pragma once
#include "oracle_chain.hpp"
#include "native_fx.hpp"
#include "engine.hpp"
#include <mutex>
#include <thread>
namespace vamp {
// UI owns the chains. An audio reader never releases a plugin or allocates a chain.
class OracleRack {
public:
    struct Stage{AssistantStep step;std::shared_ptr<Plugin>effect;std::string error;Json missingState;};
    struct Chain{std::vector<Stage>stages;int latency=0;StereoDelay dry{0};};
    NativeEffect oracle{46,48000};
    std::atomic<double>rate{48000},transportTempo{120},transportBeat{0};std::atomic<bool>bypass{false};std::atomic<int>latency{0};
    std::function<void()>dirty,latencyChanged;
    Json catalog=Json::array();std::vector<fs::path>roots;fs::path data,moduleDirectory;
    std::wstring scanMessage;std::atomic<int>scanTotal{0},scanCount{0};std::atomic<bool>scanRunning{false},scanCancel{false};
    OracleRack();~OracleRack();
    bool apply(const AssistantPlan&);bool prepare(double);void process(float*l,float*r,int frames,double beat,double tempo,bool playing);
    Json state();bool restore(const Json&);void poll();void scan();void libraryMenu();
    Chain*chain()const{return active.load(std::memory_order_acquire);}
private:
    std::atomic<Chain*>active{nullptr};std::atomic<unsigned>readers{0};std::atomic<bool>editing{false};
    std::vector<std::unique_ptr<Chain>>owned;std::mutex uiMutex;std::thread scanner;
    std::mutex scanMutex;Json scanResults=Json::array();bool scanReady=false;bool pendingLatency=false;
    std::array<float,MaxBlock>dryL{},dryR{};double wet=1;
    void publish(std::unique_ptr<Chain>);void collect();void saveLibrary();void readLibrary();
};
}
