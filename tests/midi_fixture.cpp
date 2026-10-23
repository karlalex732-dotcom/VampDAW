#define INIT_CLASS_IID
#define VAMP_FACTORY_FIXTURE
#include "engine.hpp"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "plugin_bus_tests.hpp"
#include <cstring>
using namespace vamp;namespace SB=Steinberg;namespace V=Steinberg::Vst;
namespace {
class MidiFixture final:public BusTestPlugin,public V::IEditController,public V::IMidiMapping{
    SB::uint32 refs=1;std::array<double,6>values{};
public:
    MidiFixture(){layouts({},{bus(2)});}
    SB::tresult PLUGIN_API queryInterface(const SB::TUID id,void**out)override{*out=nullptr;if(!memcmp(id,V::IComponent_iid,16)||!memcmp(id,SB::FUnknown_iid,16))*out=static_cast<V::IComponent*>(this);else if(!memcmp(id,V::IAudioProcessor_iid,16))*out=static_cast<V::IAudioProcessor*>(this);else if(!memcmp(id,V::IEditController_iid,16))*out=static_cast<V::IEditController*>(this);else if(!memcmp(id,V::IMidiMapping_iid,16))*out=static_cast<V::IMidiMapping*>(this);if(!*out)return SB::kNoInterface;addRef();return 0;}
    SB::uint32 PLUGIN_API addRef()override{return++refs;}SB::uint32 PLUGIN_API release()override{auto n=--refs;if(!n)delete this;return n;}
    SB::tresult PLUGIN_API initialize(SB::FUnknown*)override{return 0;}SB::tresult PLUGIN_API terminate()override{return 0;}
    SB::int32 PLUGIN_API getBusCount(V::MediaType t,V::BusDirection d)override{return t==V::kEvent?(d==V::kInput?1:0):BusTestPlugin::getBusCount(t,d);}
    SB::tresult PLUGIN_API getBusInfo(V::MediaType t,V::BusDirection d,SB::int32 index,V::BusInfo&info)override{if(t==V::kEvent){info={};info.mediaType=t;info.direction=d;info.channelCount=16;return 0;}return BusTestPlugin::getBusInfo(t,d,index,info);}
    SB::tresult PLUGIN_API activateBus(V::MediaType t,V::BusDirection d,SB::int32 i,SB::TBool a)override{return t==V::kEvent?0:BusTestPlugin::activateBus(t,d,i,a);}
    SB::tresult PLUGIN_API getMidiControllerAssignment(SB::int32 bus,SB::int16 channel,V::CtrlNumber cc,V::ParamID&id)override{if(bus||channel!=2)return SB::kResultFalse;int index=cc==1?0:cc==64?1:cc==129?2:cc==128?3:-1;if(index<0)return SB::kResultFalse;id=index;return 0;}
    SB::int32 PLUGIN_API getParameterCount()override{return 6;}
    SB::tresult PLUGIN_API getParameterInfo(SB::int32 i,V::ParameterInfo&info)override{if(i<0||i>=6)return SB::kInvalidArgument;info={};info.id=i;info.flags=V::ParameterInfo::kCanAutomate;info.title[0]=u'P';info.title[1]=u'0'+i;return 0;}
    SB::tresult PLUGIN_API setComponentState(SB::IBStream*)override{return 0;}SB::tresult PLUGIN_API setState(SB::IBStream*)override{return 0;}SB::tresult PLUGIN_API getState(SB::IBStream*)override{return 0;}
    SB::tresult PLUGIN_API getParamStringByValue(V::ParamID,V::ParamValue,V::String128)override{return SB::kNotImplemented;}SB::tresult PLUGIN_API getParamValueByString(V::ParamID,V::TChar*,V::ParamValue&)override{return SB::kNotImplemented;}
    V::ParamValue PLUGIN_API normalizedParamToPlain(V::ParamID,V::ParamValue x)override{return x;}V::ParamValue PLUGIN_API plainParamToNormalized(V::ParamID,V::ParamValue x)override{return x;}V::ParamValue PLUGIN_API getParamNormalized(V::ParamID id)override{return id<values.size()?values[id]:0;}
    SB::tresult PLUGIN_API setParamNormalized(V::ParamID id,V::ParamValue x)override{if(id>=values.size())return SB::kInvalidArgument;values[id]=x;return 0;}SB::tresult PLUGIN_API setComponentHandler(V::IComponentHandler*)override{return 0;}SB::IPlugView*PLUGIN_API createView(SB::FIDString)override{return nullptr;}
    SB::tresult PLUGIN_API process(V::ProcessData&data)override{if(data.inputParameterChanges)for(int i=0;i<data.inputParameterChanges->getParameterCount();i++){auto*q=data.inputParameterChanges->getParameterData(i);if(q&&q->getParameterId()<values.size()){SB::int32 offset;double value;if(q->getPoint(q->getPointCount()-1,offset,value)==0)values[q->getParameterId()]=value;}}if(data.inputEvents)for(int i=0;i<data.inputEvents->getEventCount();i++){V::Event e{};if(data.inputEvents->getEvent(i,e)==0&&e.type==V::Event::kPolyPressureEvent)values[4]=e.polyPressure.pressure;}for(int i=0;i<data.numSamples;i++)data.outputs[0].channelBuffers32[0][i]=data.outputs[0].channelBuffers32[1][i]=(float)(values[0]*.1+values[1]*.2+values[2]*.3+values[3]*.15+values[4]*.1+values[5]*.05);return 0;}
};
class Factory final:public SB::IPluginFactory{
public:
    SB::tresult PLUGIN_API queryInterface(const SB::TUID id,void**out)override{*out=nullptr;if(memcmp(id,SB::IPluginFactory_iid,16)&&memcmp(id,SB::FUnknown_iid,16))return SB::kNoInterface;*out=this;return 0;}SB::uint32 PLUGIN_API addRef()override{return 1;}SB::uint32 PLUGIN_API release()override{return 1;}
    SB::tresult PLUGIN_API getFactoryInfo(SB::PFactoryInfo*info)override{*info={};strcpy(info->vendor,"Karsei Test");return 0;}SB::int32 PLUGIN_API countClasses()override{return 1;}SB::tresult PLUGIN_API getClassInfo(SB::int32 i,SB::PClassInfo*info)override{if(i)return SB::kInvalidArgument;*info={};memcpy(info->cid,"VampMidiTest0001",16);strcpy(info->category,kVstAudioEffectClass);strcpy(info->name,"MIDI Mapping Fixture");return 0;}SB::tresult PLUGIN_API createInstance(SB::FIDString,SB::FIDString id,void**out)override{auto*p=new MidiFixture;auto result=p->queryInterface(id,out);p->release();return result;}
}factory;
}
extern "C" __declspec(dllexport) SB::IPluginFactory*PLUGIN_API GetPluginFactory(){return &factory;}
extern "C" __declspec(dllexport) bool PLUGIN_API InitDll(){return true;}
extern "C" __declspec(dllexport) bool PLUGIN_API ExitDll(){return true;}
