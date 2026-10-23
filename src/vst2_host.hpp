#pragma once
#include "plugin.hpp"
namespace vamp {
class Vst2Host{
    struct Impl;std::unique_ptr<Impl>h;
public:
    PluginInfo info;
    Vst2Host();~Vst2Host();bool load(const fs::path&,double,std::string&);bool prepare(double);
    void process(float*,float*,int,const PluginNote*,int,double,double,bool,const std::vector<Point>*,uint32_t,const float*,const float*,const std::vector<AutomationLane>*,double,double);
    void midiMessage(uint32_t);bool setParameter(uint32_t,double);double parameterValue(uint32_t)const;
    std::string parameterDisplay(uint32_t,double);
    std::vector<PluginParameter>parameters();Json state();void restore(const Json&);Json buses()const;
    void editor(HWND);void hideEditor();bool takeChanges();bool needsRestart();int latency()const;bool acceptsNotes()const;
};
}
