#define INIT_CLASS_IID
#define VAMP_FACTORY_FIXTURE
#include "engine.hpp"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/base/ibstream.h"
#include "plugin_bus_tests.hpp"
#include <cstring>
using namespace vamp;
namespace SB=Steinberg;namespace V=Steinberg::Vst;
namespace {
class FixturePlugin final:public BusTestPlugin {
    SB::uint32 refs=1;SB::tresult result=SB::kResultOk;bool failSetup=false,failActive=false;std::array<uint8_t,257>saved{};
public:
    FixturePlugin(){layouts({bus(2)},{bus(2)});wchar_t value[32]{};GetEnvironmentVariableW(L"VAMP_FIXTURE_RESULT",value,32);if(!wcscmp(value,L"false"))result=SB::kResultFalse;else if(!wcscmp(value,L"notimpl"))result=SB::kNotImplemented;else if(!wcscmp(value,L"error"))result=SB::kInternalError;failSetup=GetEnvironmentVariableW(L"VAMP_FIXTURE_SETUP",value,32)>0;failActive=GetEnvironmentVariableW(L"VAMP_FIXTURE_ACTIVE",value,32)>0;}
    SB::tresult PLUGIN_API queryInterface(const SB::TUID iid,void**object)override{*object=nullptr;if(!memcmp(iid,V::IComponent_iid,16)||!memcmp(iid,SB::FUnknown_iid,16))*object=static_cast<V::IComponent*>(this);else if(!memcmp(iid,V::IAudioProcessor_iid,16))*object=static_cast<V::IAudioProcessor*>(this);if(!*object)return SB::kNoInterface;addRef();return 0;}
    SB::uint32 PLUGIN_API addRef()override{return++refs;}
    SB::uint32 PLUGIN_API release()override{auto n=--refs;if(!n)delete this;return n;}
    SB::tresult PLUGIN_API setProcessing(SB::TBool active)override{return active?result:SB::kResultOk;}
    SB::tresult PLUGIN_API setupProcessing(V::ProcessSetup&s)override{return failSetup?SB::kInternalError:BusTestPlugin::setupProcessing(s);}
    SB::tresult PLUGIN_API setActive(SB::TBool active)override{return active&&failActive?SB::kInternalError:SB::kResultOk;}
    SB::tresult PLUGIN_API getState(SB::IBStream*stream)override{SB::int32 written=0;return stream->write(saved.data(),(SB::int32)saved.size(),&written)==0&&written==saved.size()?SB::kResultOk:SB::kInternalError;}
    SB::tresult PLUGIN_API setState(SB::IBStream*stream)override{SB::int32 read=0;return stream->read(saved.data(),(SB::int32)saved.size(),&read)==0&&read==saved.size()?SB::kResultOk:SB::kInternalError;}
    SB::tresult PLUGIN_API process(V::ProcessData&data)override{for(int c=0;c<2;c++)for(int n=0;n<data.numSamples;n++)data.outputs[0].channelBuffers32[c][n]=data.inputs[0].channelBuffers32[c][n];return 0;}
};
class Factory final:public SB::IPluginFactory {
public:
    SB::tresult PLUGIN_API queryInterface(const SB::TUID iid,void**object)override{*object=nullptr;if(memcmp(iid,SB::IPluginFactory_iid,16)&&memcmp(iid,SB::FUnknown_iid,16))return SB::kNoInterface;*object=this;return 0;}
    SB::uint32 PLUGIN_API addRef()override{return 1;}SB::uint32 PLUGIN_API release()override{return 1;}
    SB::tresult PLUGIN_API getFactoryInfo(SB::PFactoryInfo*info)override{*info={};strcpy(info->vendor,"Karsei Test");return 0;}
    SB::int32 PLUGIN_API countClasses()override{return 1;}
    SB::tresult PLUGIN_API getClassInfo(SB::int32 index,SB::PClassInfo*info)override{if(index)return SB::kInvalidArgument;*info={};memcpy(info->cid,"VampLifeCycle001",16);strcpy(info->category,kVstAudioEffectClass);strcpy(info->name,"Processing Status Fixture");return 0;}
    SB::tresult PLUGIN_API createInstance(SB::FIDString,SB::FIDString iid,void**object)override{auto*p=new FixturePlugin;auto result=p->queryInterface(iid,object);p->release();return result;}
}factory;
}
extern "C" __declspec(dllexport) SB::IPluginFactory* PLUGIN_API GetPluginFactory(){return &factory;}
extern "C" __declspec(dllexport) bool PLUGIN_API InitDll(){return true;}
extern "C" __declspec(dllexport) bool PLUGIN_API ExitDll(){return true;}
