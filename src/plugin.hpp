#pragma once
#include "model.hpp"
#include <functional>

namespace vamp {
class NativeEffect;
class Vst2Host;
struct AraTrackContext;struct AraTransportRequest;
std::vector<Json>nativePluginCatalog();
struct PluginNote { int offset=0,pitch=60,id=0;float velocity=.7f;bool on=true;int channel=0; };
struct PluginInfo {std::string name, vendor,category;bool instrument=false;bool araAvailable=false;};
struct PluginParameter{uint32_t id;std::string name,unit;};
class Plugin {
public:
    struct Impl; std::unique_ptr<Impl> impl;std::unique_ptr<NativeEffect>native;std::unique_ptr<Vst2Host>vst2;
    Plugin();~Plugin();Plugin(const Plugin&)=delete;Plugin&operator=(const Plugin&)=delete;
    bool load(const fs::path&path,double rate,std::string&error,const AraTrackContext*ara=nullptr);
    bool araActive()const;bool araNeedsUpdate(const AraTrackContext&)const;void syncAra(const AraTrackContext&);AraTransportRequest pollAra();double araProgress()const;void selectAraClip(const std::string&);
    void process(float*left,float*right,int frames,const PluginNote*notes,int count,double beat,double tempo,bool playing,const std::vector<Point>*automation=nullptr,uint32_t param=0,const float*sideL=nullptr,const float*sideR=nullptr,const std::vector<AutomationLane>*lanes=nullptr,double loopLength=0,double loopStart=0);
    void editor(HWND parent);void hideEditor();
    Json state();void restore(const Json&state);
    int latency()const; bool instrument()const;bool acceptsNotes()const;
    Json audioBusLayout()const;
    bool takeChanges();std::vector<PluginParameter> parameters();void displayParameter(uint32_t id,double value);
    bool setParameter(uint32_t id,double value);double parameterValue(uint32_t id)const;
    bool setParameterText(uint32_t id,const std::string&text);
    std::string parameterDisplay(uint32_t id,double normalized);
    bool setParameterPhysical(uint32_t id,double value,const std::string&unit);
    void midiMessage(uint32_t message); // Called only by the audio renderer.
    bool needsRestart();bool prepare(double rate);
    PluginInfo info;fs::path path;
};
Json describePlugin(const fs::path&path);
std::vector<fs::path> findPlugins(const std::vector<fs::path>& roots);
}
