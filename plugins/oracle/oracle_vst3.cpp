#include "oracle_rack.hpp"
#include "plugin_brand.hpp"
#include "native_window_style.hpp"
#include "localization.hpp"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <cstring>
#include <sstream>
#include <iomanip>
using namespace vamp;namespace SB=Steinberg;namespace V=Steinberg::Vst;
namespace {
const SB::FUID processorId(0x4B617273,0x65694F72,0x61636C65,0x52616331);
std::atomic<int>instanceCount{0};HMODULE dllModule=nullptr;
bool equalId(const char*a,const char*b){return a&&b&&!memcmp(a,b,16);}
void text128(SB::char16*out,const std::wstring&s){std::fill_n(out,128,0);memcpy(out,s.data(),std::min<size_t>(127,s.size())*2);}
class OraclePlugin;
class OracleView final:public SB::IPlugView {
    std::atomic<SB::uint32>refs{1};OraclePlugin*owner;HWND child=nullptr;SB::IPlugFrame*frame=nullptr;SB::ViewRect size{0,0,1000,760};
public:
    explicit OracleView(OraclePlugin*);~OracleView();
    SB::tresult PLUGIN_API queryInterface(const SB::TUID id,void**out)override{if(!out)return SB::kInvalidArgument;*out=nullptr;if(equalId(id,SB::IPlugView_iid)||equalId(id,SB::FUnknown_iid))*out=static_cast<SB::IPlugView*>(this);if(!*out)return SB::kNoInterface;addRef();return 0;}
    SB::uint32 PLUGIN_API addRef()override{return++refs;}SB::uint32 PLUGIN_API release()override{auto n=--refs;if(!n)delete this;return n;}
    SB::tresult PLUGIN_API isPlatformTypeSupported(SB::FIDString type)override{return type&&!strcmp(type,SB::kPlatformTypeHWND)?0:SB::kResultFalse;}
    SB::tresult PLUGIN_API attached(void*,SB::FIDString)override;SB::tresult PLUGIN_API removed()override;
    SB::tresult PLUGIN_API onWheel(float distance)override{if(child)SendMessageW(child,WM_MOUSEWHEEL,MAKEWPARAM(0,(short)(distance*120)),0);return 0;}
    SB::tresult PLUGIN_API onKeyDown(SB::char16,SB::int16,SB::int16)override{return SB::kResultFalse;}SB::tresult PLUGIN_API onKeyUp(SB::char16,SB::int16,SB::int16)override{return SB::kResultFalse;}
    SB::tresult PLUGIN_API getSize(SB::ViewRect*r)override{if(!r)return SB::kInvalidArgument;*r=size;return 0;}
    SB::tresult PLUGIN_API onSize(SB::ViewRect*r)override{if(!r||r->right-r->left<720||r->bottom-r->top<640)return SB::kInvalidArgument;size=*r;if(child)MoveWindow(child,0,0,r->right-r->left,r->bottom-r->top,TRUE);return 0;}
    SB::tresult PLUGIN_API onFocus(SB::TBool state)override{if(state&&child)SetFocus(child);return 0;}
    SB::tresult PLUGIN_API setFrame(SB::IPlugFrame*f)override{if(f)f->addRef();if(frame)frame->release();frame=f;return 0;}
    SB::tresult PLUGIN_API canResize()override{return 0;}
    SB::tresult PLUGIN_API checkSizeConstraint(SB::ViewRect*r)override{if(!r)return SB::kInvalidArgument;r->right=r->left+(int)clamp(r->right-r->left,720,1800);r->bottom=r->top+(int)clamp(r->bottom-r->top,640,1200);return 0;}
};
class OraclePlugin final:public V::IComponent,public V::IAudioProcessor,public V::IEditController {
    std::atomic<SB::uint32>refs{1};SB::FUnknown*context=nullptr;V::IComponentHandler*handler=nullptr;bool initialized=false,active=false;V::SpeakerArrangement arrangement=V::SpeakerArr::kStereo;int sampleSize=V::kSample32,maxBlock=4096;
    std::array<float,MaxBlock>left{},right{};std::array<double,3>lastValues{};
public:
    OracleRack rack;ULONG_PTR gdip=0;
    OraclePlugin(){instanceCount++;ui::language=0;Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&gdip,&input,nullptr);for(int i=0;i<3;i++)lastValues[i]=rack.oracle.values[i];rack.dirty=[this]{if(!handler)return;for(int i=0;i<3;i++){double value=rack.oracle.values[i];if(value!=lastValues[i]){handler->beginEdit(i);handler->performEdit(i,value);handler->endEdit(i);lastValues[i]=value;}}V::IComponentHandler2*extra=nullptr;if(handler->queryInterface(V::IComponentHandler2_iid,(void**)&extra)==0){extra->setDirty(true);extra->release();}};rack.latencyChanged=[this]{if(handler)handler->restartComponent(V::kLatencyChanged);};}
    ~OraclePlugin(){if(rack.oracle.window)DestroyWindow(rack.oracle.window);rack.oracle.window=nullptr;if(handler)handler->release();if(context)context->release();if(gdip)Gdiplus::GdiplusShutdown(gdip);instanceCount--;}
    SB::tresult PLUGIN_API queryInterface(const SB::TUID id,void**out)override{if(!out)return SB::kInvalidArgument;*out=nullptr;if(equalId(id,V::IComponent_iid)||equalId(id,SB::IPluginBase_iid)||equalId(id,SB::FUnknown_iid))*out=static_cast<V::IComponent*>(this);else if(equalId(id,V::IAudioProcessor_iid))*out=static_cast<V::IAudioProcessor*>(this);else if(equalId(id,V::IEditController_iid))*out=static_cast<V::IEditController*>(this);if(!*out)return SB::kNoInterface;addRef();return 0;}
    SB::uint32 PLUGIN_API addRef()override{return++refs;}SB::uint32 PLUGIN_API release()override{auto n=--refs;if(!n)delete this;return n;}
    SB::tresult PLUGIN_API initialize(SB::FUnknown*c)override{if(initialized)return SB::kResultFalse;initialized=true;context=c;if(c)c->addRef();return 0;}
    SB::tresult PLUGIN_API terminate()override{if(rack.oracle.window)DestroyWindow(rack.oracle.window);if(context){context->release();context=nullptr;}initialized=false;return 0;}
    SB::tresult PLUGIN_API getControllerClassId(SB::TUID)override{return SB::kNoInterface;}
    SB::tresult PLUGIN_API setIoMode(V::IoMode)override{return 0;}
    SB::int32 PLUGIN_API getBusCount(V::MediaType type,V::BusDirection)override{return type==V::kAudio?1:0;}
    SB::tresult PLUGIN_API getBusInfo(V::MediaType type,V::BusDirection dir,SB::int32 index,V::BusInfo&info)override{if(type!=V::kAudio||index||!(dir==V::kInput||dir==V::kOutput))return SB::kInvalidArgument;info={};info.mediaType=type;info.direction=dir;info.channelCount=arrangement==V::SpeakerArr::kMono?1:2;info.busType=V::kMain;info.flags=V::BusInfo::kDefaultActive;text128(info.name,dir==V::kInput?L"Oracle Input":L"Oracle Output");return 0;}
    SB::tresult PLUGIN_API getRoutingInfo(V::RoutingInfo&in,V::RoutingInfo&out)override{if(in.mediaType!=V::kAudio||in.busIndex)return SB::kResultFalse;out=in;return 0;}
    SB::tresult PLUGIN_API activateBus(V::MediaType type,V::BusDirection,SB::int32 index,SB::TBool)override{return type==V::kAudio&&index==0?0:SB::kInvalidArgument;}
    SB::tresult PLUGIN_API setActive(SB::TBool state)override{active=state;if(state&&!rack.prepare(rack.rate))return SB::kResultFalse;return 0;}
    SB::tresult PLUGIN_API setState(SB::IBStream*stream)override{if(!stream)return SB::kInvalidArgument;try{uint32_t length=0;SB::int32 count=0;if(stream->read(&length,4,&count)!=0||count!=4||length>256u*1024*1024)return SB::kResultFalse;std::string data(length,0);size_t at=0;while(at<data.size()){int n=(int)std::min<size_t>(1024*1024,data.size()-at);count=0;auto result=stream->read(data.data()+at,n,&count);if(count<=0||count>n)return SB::kResultFalse;at+=count;if(result!=0&&at<data.size())return SB::kResultFalse;}auto j=Json::parse(data);return rack.restore(j)?0:SB::kResultFalse;}catch(...){return SB::kResultFalse;}}
    SB::tresult PLUGIN_API getState(SB::IBStream*stream)override{if(!stream)return SB::kInvalidArgument;try{auto data=rack.state().dump();if(data.size()>256u*1024*1024)return SB::kResultFalse;uint32_t length=(uint32_t)data.size();SB::int32 count=0;if(stream->write(&length,4,&count)!=0||count!=4)return SB::kResultFalse;size_t at=0;while(at<data.size()){int n=(int)std::min<size_t>(1024*1024,data.size()-at);count=0;if(stream->write(data.data()+at,n,&count)!=0||count<=0||count>n)return SB::kResultFalse;at+=count;}return 0;}catch(...){return SB::kResultFalse;}}
    SB::tresult PLUGIN_API setComponentState(SB::IBStream*)override{return 0;}
    SB::tresult PLUGIN_API setBusArrangements(V::SpeakerArrangement*in,SB::int32 ni,V::SpeakerArrangement*out,SB::int32 no)override{if(ni!=1||no!=1||!in||!out||in[0]!=out[0]||(in[0]!=V::SpeakerArr::kMono&&in[0]!=V::SpeakerArr::kStereo))return SB::kResultFalse;arrangement=in[0];return 0;}
    SB::tresult PLUGIN_API getBusArrangement(V::BusDirection,SB::int32 index,V::SpeakerArrangement&out)override{if(index)return SB::kInvalidArgument;out=arrangement;return 0;}
    SB::tresult PLUGIN_API canProcessSampleSize(SB::int32 size)override{return size==V::kSample32||size==V::kSample64?0:SB::kResultFalse;}
    SB::uint32 PLUGIN_API getLatencySamples()override{return rack.latency.load();}
    SB::tresult PLUGIN_API setupProcessing(V::ProcessSetup&setup)override{if(setup.maxSamplesPerBlock<1||setup.maxSamplesPerBlock>1048576||canProcessSampleSize(setup.symbolicSampleSize)!=0)return SB::kInvalidArgument;maxBlock=setup.maxSamplesPerBlock;sampleSize=setup.symbolicSampleSize;return rack.prepare(setup.sampleRate)?0:SB::kResultFalse;}
    SB::tresult PLUGIN_API setProcessing(SB::TBool)override{return 0;}
    SB::tresult PLUGIN_API process(V::ProcessData&data)override{
        if(data.numSamples<0||data.numSamples>maxBlock||data.symbolicSampleSize!=sampleSize)return SB::kInvalidArgument;
        std::array<V::IParamValueQueue*,4>queues{};std::array<int,4>positions{};
        if(data.inputParameterChanges)for(int q=0;q<data.inputParameterChanges->getParameterCount();q++){auto*queue=data.inputParameterChanges->getParameterData(q);if(queue&&queue->getParameterId()<4)queues[queue->getParameterId()]=queue;}
        if(!data.numSamples)for(auto*queue:queues)if(queue&&queue->getPointCount()){int offset=0;double value=0;if(queue->getPoint(queue->getPointCount()-1,offset,value)==0)setParamNormalized(queue->getParameterId(),value);}
        if(!data.numSamples||!data.numOutputs||!data.outputs)return 0;auto&out=data.outputs[0];auto*in=data.numInputs&&data.inputs?&data.inputs[0]:nullptr;int channels=arrangement==V::SpeakerArr::kMono?1:2;if(out.numChannels!=channels)return SB::kInvalidArgument;
        double tempo=120,beat=0;bool playing=false;if(data.processContext){auto&c=*data.processContext;if((c.state&V::ProcessContext::kTempoValid)&&c.tempo>0&&std::isfinite(c.tempo))tempo=c.tempo;if(c.state&V::ProcessContext::kProjectTimeMusicValid)beat=c.projectTimeMusic;playing=c.state&V::ProcessContext::kPlaying;}
        for(int first=0;first<data.numSamples;){int stop=std::min(data.numSamples,first+MaxBlock);for(size_t q=0;q<queues.size();q++)if(auto*queue=queues[q]){while(positions[q]<queue->getPointCount()){int offset=0;double value=0;if(queue->getPoint(positions[q],offset,value)!=0){positions[q]++;continue;}if(offset<=first){setParamNormalized((V::ParamID)q,value);positions[q]++;}else{stop=std::min(stop,offset);break;}}}int n=std::max(1,stop-first);for(int i=0;i<n;i++){if(sampleSize==V::kSample32){left[i]=in&&in->numChannels>0&&in->channelBuffers32&&in->channelBuffers32[0]?in->channelBuffers32[0][first+i]:0;right[i]=in&&in->numChannels>1&&in->channelBuffers32&&in->channelBuffers32[1]?in->channelBuffers32[1][first+i]:left[i];}else{left[i]=in&&in->numChannels>0&&in->channelBuffers64&&in->channelBuffers64[0]?(float)in->channelBuffers64[0][first+i]:0;right[i]=in&&in->numChannels>1&&in->channelBuffers64&&in->channelBuffers64[1]?(float)in->channelBuffers64[1][first+i]:left[i];}}
            rack.process(left.data(),right.data(),n,beat+first*tempo/(60*rack.rate),tempo,playing);
            for(int i=0;i<n;i++)for(int c=0;c<channels;c++){float value=channels==1?(left[i]+right[i])*.5f:c?right[i]:left[i];if(sampleSize==V::kSample32){if(out.channelBuffers32&&out.channelBuffers32[c])out.channelBuffers32[c][first+i]=value;}else if(out.channelBuffers64&&out.channelBuffers64[c])out.channelBuffers64[c][first+i]=value;}first+=n;}
        out.silenceFlags=0;return 0;
    }
    SB::uint32 PLUGIN_API getTailSamples()override{return rack.chain()&&!rack.chain()->stages.empty()?V::kInfiniteTail:0;}
    SB::int32 PLUGIN_API getParameterCount()override{return 4;}
    SB::tresult PLUGIN_API getParameterInfo(SB::int32 index,V::ParameterInfo&info)override{if(index<0||index>=4)return SB::kInvalidArgument;info={};info.id=index;info.flags=index==3?V::ParameterInfo::kCanAutomate:0;if(index<3){auto&p=rack.oracle.spec().parameters[index];text128(info.title,wide(p.name));text128(info.shortTitle,wide(p.name));text128(info.units,wide(p.unit));info.stepCount=p.stepped?(int)(p.high-p.low):0;info.defaultNormalizedValue=p.normalized(p.initial);}else{text128(info.title,L"Bypass");text128(info.shortTitle,L"Bypass");info.flags|=V::ParameterInfo::kIsBypass;info.stepCount=1;}return 0;}
    SB::tresult PLUGIN_API getParamStringByValue(V::ParamID id,V::ParamValue value,V::String128 out)override{if(id>=4||!std::isfinite(value))return SB::kInvalidArgument;std::wostringstream s;if(id==3)s<<(value>=.5?L"On":L"Off");else if(id==0)s<<wide(assistantPresets()[(int)rack.oracle.spec().parameters[0].value(value)].name);else s<<std::fixed<<std::setprecision(id==2?0:1)<<rack.oracle.spec().parameters[id].value(value);text128(out,s.str());return 0;}
    SB::tresult PLUGIN_API getParamValueByString(V::ParamID id,V::TChar*text,V::ParamValue&value)override{if(id>=4||!text)return SB::kInvalidArgument;try{auto input=std::wstring((wchar_t*)text);if(id==3){value=input==L"On"||input==L"1"?1:0;return 0;}if(id==0)for(size_t i=0;i<assistantPresets().size();i++)if(input==wide(assistantPresets()[i].name)){value=rack.oracle.spec().parameters[0].normalized(i);return 0;}size_t used=0;double real=std::stod(input,&used);if(used!=input.size()||!std::isfinite(real))return SB::kResultFalse;value=rack.oracle.spec().parameters[id].normalized(real);return 0;}catch(...){return SB::kResultFalse;}}
    V::ParamValue PLUGIN_API normalizedParamToPlain(V::ParamID id,V::ParamValue value)override{return id<3?rack.oracle.spec().parameters[id].value(value):value>=.5?1:0;}
    V::ParamValue PLUGIN_API plainParamToNormalized(V::ParamID id,V::ParamValue value)override{return id<3?rack.oracle.spec().parameters[id].normalized(value):value>=.5?1:0;}
    V::ParamValue PLUGIN_API getParamNormalized(V::ParamID id)override{return id<3?rack.oracle.values[id].load():id==3&&rack.bypass?1:0;}
    SB::tresult PLUGIN_API setParamNormalized(V::ParamID id,V::ParamValue value)override{if(id>=4||!std::isfinite(value))return SB::kInvalidArgument;if(id<3)rack.oracle.set(id,value);else rack.bypass=value>=.5;return 0;}
    SB::tresult PLUGIN_API setComponentHandler(V::IComponentHandler*h)override{if(h)h->addRef();if(handler)handler->release();handler=h;return 0;}
    SB::IPlugView* PLUGIN_API createView(SB::FIDString name)override{return name&&!strcmp(name,V::ViewType::kEditor)?new OracleView(this):nullptr;}
};
OracleView::OracleView(OraclePlugin*p):owner(p){owner->addRef();}
OracleView::~OracleView(){removed();if(frame)frame->release();owner->release();}
SB::tresult OracleView::attached(void*parent,SB::FIDString type){if(child||!parent||isPlatformTypeSupported(type)!=0)return SB::kResultFalse;auto&fx=owner->rack.oracle;if(fx.window)return SB::kResultFalse;WNDCLASSW cls{};cls.lpfnWndProc=NativeEffect::windowProc;cls.hInstance=dllModule;cls.lpszClassName=L"Karsei.OracleRack.Editor.1";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);child=CreateWindowExW(0,cls.lpszClassName,L"Oracle Rack",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_VSCROLL,0,0,size.right-size.left,size.bottom-size.top,(HWND)parent,nullptr,dllModule,&fx);if(!child)return SB::kResultFalse;fx.window=child;ui::styleNativePanel(child);SetTimer(child,1,66,nullptr);return 0;}
SB::tresult OracleView::removed(){if(child){for(auto&s:owner->rack.chain()->stages)if(s.effect)s.effect->hideEditor();if(IsWindow(child))DestroyWindow(child);child=nullptr;owner->rack.oracle.window=nullptr;}return 0;}
class OracleFactory final:public SB::IPluginFactory2 {
    std::atomic<SB::uint32>refs{1};
public:
    SB::tresult PLUGIN_API queryInterface(const SB::TUID id,void**out)override{if(!out)return SB::kInvalidArgument;*out=nullptr;if(equalId(id,SB::IPluginFactory_iid)||equalId(id,SB::IPluginFactory2_iid)||equalId(id,SB::FUnknown_iid))*out=static_cast<SB::IPluginFactory2*>(this);if(!*out)return SB::kNoInterface;addRef();return 0;}
    SB::uint32 PLUGIN_API addRef()override{return++refs;}SB::uint32 PLUGIN_API release()override{auto n=--refs;return n;}
    SB::tresult PLUGIN_API getFactoryInfo(SB::PFactoryInfo*info)override{if(!info)return SB::kInvalidArgument;*info={};strcpy(info->vendor,"Karsei");info->flags=SB::PFactoryInfo::kUnicode;return 0;}
    SB::int32 PLUGIN_API countClasses()override{return 1;}
    SB::tresult PLUGIN_API getClassInfo(SB::int32 index,SB::PClassInfo*info)override{if(index||!info)return SB::kInvalidArgument;*info={};processorId.toTUID(info->cid);info->cardinality=SB::PClassInfo::kManyInstances;strcpy(info->category,kVstAudioEffectClass);strcpy(info->name,"Oracle Rack");return 0;}
    SB::tresult PLUGIN_API getClassInfo2(SB::int32 index,SB::PClassInfo2*info)override{if(index||!info)return SB::kInvalidArgument;*info={};processorId.toTUID(info->cid);info->cardinality=SB::PClassInfo::kManyInstances;strcpy(info->category,kVstAudioEffectClass);strcpy(info->name,"Oracle Rack");strcpy(info->subCategories,"Fx|Tools|Analyzer");strcpy(info->vendor,"Karsei");strcpy(info->version,"0.1.0");strcpy(info->sdkVersion,"VST 3.7");return 0;}
    SB::tresult PLUGIN_API createInstance(SB::FIDString cid,SB::FIDString id,void**out)override{if(!out)return SB::kInvalidArgument;*out=nullptr;SB::TUID expected;processorId.toTUID(expected);if(!equalId(cid,expected))return SB::kNoInterface;try{auto*p=new OraclePlugin;auto result=p->queryInterface(id,out);p->release();return result;}catch(...){return SB::kInternalError;}}
}factory;
}
extern "C" BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){dllModule=module;DisableThreadLibraryCalls(module);}return TRUE;}
extern "C" __declspec(dllexport) SB::IPluginFactory* PLUGIN_API GetPluginFactory(){factory.addRef();return &factory;}
extern "C" __declspec(dllexport) bool PLUGIN_API InitDll(){return true;}
extern "C" __declspec(dllexport) bool PLUGIN_API ExitDll(){if(instanceCount.load()!=0)return false;for(auto cls:{L"Karsei.OracleRack.Editor.1",nativeEffectClass,hostedVst3Class,hostedVst2Class,pluginPromptClass})UnregisterClassW(cls,dllModule);return true;}
