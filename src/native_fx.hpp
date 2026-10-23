#pragma once
#include "model.hpp"
#include "assistant.hpp"
#include "hover_motion.hpp"
#include <functional>

namespace vamp {
struct NativeParameter {
    std::string name,unit;
    double low=0,high=1,initial=0;
    bool logarithmic=false,stepped=false;
    double value(double normalized)const;
    double normalized(double value)const;
};
struct NativeSpec {
    std::string id,name,category,description;
    std::vector<NativeParameter>parameters;
};
const std::vector<NativeSpec>&nativeSpecs();
int nativeEffectIndex(const fs::path&path);
std::vector<Json>nativePluginCatalog();
class NativeEffect {
public:
    struct State;std::unique_ptr<State>dsp;
    explicit NativeEffect(int kind,double rate);
    ~NativeEffect();
    NativeEffect(const NativeEffect&)=delete;
    int kind=0;double rate=48000;
    static constexpr int ParameterCapacity=32;
    std::array<std::atomic<double>,ParameterCapacity>values{};
    std::atomic<bool>edited{false};
    std::unique_ptr<AssistantCapture>assistantCapture;
    AssistantPlan assistantPlan;bool assistantAuto=true,assistantMaster=false,assistantApplied=false;
    bool assistantAnyPlugins=false,assistantStandalone=false;Json assistantOverrides=Json::object();
    Json assistantReport=Json::array();
    std::function<bool(const AssistantPlan&)>assistantApply;
    std::function<AssistantPlan(const AssistantPlan&)>assistantResolve;
    std::function<bool(int)>assistantChoose,assistantOpen;
    std::function<void()>assistantLibrary,assistantPoll;
    std::wstring assistantStatus;HWND assistantCombo=nullptr,assistantSlider=nullptr;
    std::atomic<float>inputPeak{0},outputPeak{0},reduction{0},rms{0},correlation{0};
    std::atomic<double>momentary{-100},integrated{-100};
    std::array<std::atomic<float>,1024>scope{};
    std::atomic<uint64_t>scopeHead{0};
    struct EqAnalysis{std::array<std::atomic<float>,4096>input{},inputR{},output{},outputR{};std::atomic<uint64_t>head{0};};
    std::unique_ptr<EqAnalysis>eqAnalysis;
    std::atomic<bool>analyzerRunning{false};bool analyzerEnabled=true,analyzerPre=false;int eqSelected=0;bool eqDragging=false;float eqDragY=0;double eqDragGain=0;
    std::array<float,512>eqSpectrum{};uint64_t eqSpectrumHead=~uint64_t(0);
    std::array<double,ParameterCapacity>bankA{},bankB{};bool activeB=false;
    std::atomic<int>quality{0};HWND window=nullptr;float zoom=1;int scroll=0,drag=-1;float dragX=0;double dragValue=0;
    RECT hoverRect{};ui::HoverEnvelope hoverGlow;
    const NativeSpec&spec()const;
    int latency()const;
    bool prepare(double sampleRate);
    void set(uint32_t id,double value,bool user=false);
    void process(float*l,float*r,int frames,double beat,double tempo,bool playing,
                 const std::vector<Point>*automation,uint32_t parameter,
                 const float*sideL,const float*sideR,const std::vector<AutomationLane>*lanes,
                 double loopEnd,double loopStart);
    Json state()const;void restore(const Json&);
    double eqResponseDb(double hz,int band=-1)const;
    void editor(HWND parent);void paint();
    static LRESULT CALLBACK windowProc(HWND,UINT,WPARAM,LPARAM);
};
}
