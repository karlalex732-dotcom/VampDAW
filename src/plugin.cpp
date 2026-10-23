#include "plugin_brand.hpp"
#include "window_lifecycle.hpp"
#include "engine.hpp"
#include "vst2_host.hpp"
#include "vst2_candidate.hpp"
#include <cwctype>
#include "plugin_state.hpp"
#define INIT_CLASS_IID
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include <windowsx.h>
#include <commctrl.h>
#include "pluginterfaces/vst/ivsthostapplication.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include <map>
#include <set>
#include <cstring>
#include <stdexcept>
#include "vst_audio_buses.hpp"
#include "vst_lifecycle.hpp"
#include "native_fx.hpp"
#include "parameter_physical.hpp"
#include "ara_host.hpp"

namespace vamp {
namespace SB=Steinberg; namespace V=Steinberg::Vst;
// Opt-in, flushed load diagnostics also survive a fault inside a vendor module.
static void pluginTrace(const std::string&stage){wchar_t file[32768]{};DWORD n=GetEnvironmentVariableW(L"VAMP_VST_TRACE",file,32768);if(n&&n<32768)std::ofstream(fs::path(file),std::ios::app)<<stage<<std::endl;}
inline bool equalId(const SB::TUID a,const SB::TUID b){return !memcmp(a,b,16);}
#define STATIC_INTERFACE(IFACE,IID) SB::tresult PLUGIN_API queryInterface(const SB::TUID id,void**obj)override{if(equalId(id,IID)||equalId(id,SB::FUnknown_iid)){*obj=static_cast<IFACE*>(this);addRef();return SB::kResultOk;}*obj=nullptr;return SB::kNoInterface;} SB::uint32 PLUGIN_API addRef()override{return 1;} SB::uint32 PLUGIN_API release()override{return 1;}
#define HEAP_INTERFACE(IFACE,IID) std::atomic<SB::uint32> refs{1}; SB::tresult PLUGIN_API queryInterface(const SB::TUID id,void**obj)override{if(equalId(id,IID)||equalId(id,SB::FUnknown_iid)){*obj=static_cast<IFACE*>(this);addRef();return SB::kResultOk;}*obj=nullptr;return SB::kNoInterface;} SB::uint32 PLUGIN_API addRef()override{return ++refs;} SB::uint32 PLUGIN_API release()override{auto n=--refs;if(!n)delete this;return n;}

class Attributes:public V::IAttributeList{
    struct Value{int kind=0;SB::int64 i=0;double d=0;std::u16string text;std::vector<uint8_t>bytes;};std::map<std::string,Value> values;
public:
    HEAP_INTERFACE(V::IAttributeList,V::IAttributeList_iid)
    SB::tresult PLUGIN_API setInt(AttrID id,SB::int64 x)override{auto&v=values[id];v.kind=1;v.i=x;return 0;}
    SB::tresult PLUGIN_API getInt(AttrID id,SB::int64&x)override{auto i=values.find(id);if(i==values.end()||i->second.kind!=1)return SB::kResultFalse;x=i->second.i;return 0;}
    SB::tresult PLUGIN_API setFloat(AttrID id,double x)override{auto&v=values[id];v.kind=2;v.d=x;return 0;}
    SB::tresult PLUGIN_API getFloat(AttrID id,double&x)override{auto i=values.find(id);if(i==values.end()||i->second.kind!=2)return SB::kResultFalse;x=i->second.d;return 0;}
    SB::tresult PLUGIN_API setString(AttrID id,const V::TChar*x)override{auto&v=values[id];v.kind=3;v.text=(const char16_t*)x;return 0;}
    SB::tresult PLUGIN_API getString(AttrID id,V::TChar*x,SB::uint32 bytes)override{auto i=values.find(id);if(i==values.end()||i->second.kind!=3||bytes<2)return SB::kResultFalse;size_t n=std::min<size_t>(bytes/2-1,i->second.text.size());memcpy(x,i->second.text.data(),n*2);x[n]=0;return 0;}
    SB::tresult PLUGIN_API setBinary(AttrID id,const void*x,SB::uint32 bytes)override{if(bytes>32*1024*1024)return SB::kInvalidArgument;auto&v=values[id];v.kind=4;v.bytes.assign((const uint8_t*)x,(const uint8_t*)x+bytes);return 0;}
    SB::tresult PLUGIN_API getBinary(AttrID id,const void*&x,SB::uint32&bytes)override{auto i=values.find(id);if(i==values.end()||i->second.kind!=4)return SB::kResultFalse;x=i->second.bytes.data();bytes=(SB::uint32)i->second.bytes.size();return 0;}
};
class Message:public V::IMessage{
    std::string name;Attributes*attributes=new Attributes;
public:
    HEAP_INTERFACE(V::IMessage,V::IMessage_iid)
    ~Message(){attributes->release();}
    SB::FIDString PLUGIN_API getMessageID()override{return name.c_str();}
    void PLUGIN_API setMessageID(SB::FIDString s)override{name=s?s:"";}
    V::IAttributeList* PLUGIN_API getAttributes()override{return attributes;}
};
class MemoryStream:public SB::IBStream{
public:
    std::vector<uint8_t>bytes;size_t position=0;STATIC_INTERFACE(SB::IBStream,SB::IBStream_iid)
    SB::tresult PLUGIN_API read(void*out,SB::int32 count,SB::int32*readCount)override{if(count<0)return SB::kInvalidArgument;size_t n=std::min<size_t>(count,bytes.size()-std::min(bytes.size(),position));if(n)memcpy(out,bytes.data()+position,n);position+=n;if(readCount)*readCount=(SB::int32)n;return n?0:SB::kResultFalse;}
    SB::tresult PLUGIN_API write(void*in,SB::int32 count,SB::int32*written)override{if(count<0||position+count>32*1024*1024)return SB::kInvalidArgument;bytes.resize(std::max(bytes.size(),position+count));if(count)memcpy(bytes.data()+position,in,count);position+=count;if(written)*written=count;return 0;}
    SB::tresult PLUGIN_API seek(SB::int64 offset,SB::int32 mode,SB::int64*result)override{SB::int64 target=offset+(mode==1?position:mode==2?bytes.size():0);if(target<0||target>32*1024*1024)return SB::kInvalidArgument;position=(size_t)target;if(result)*result=target;return 0;}
    SB::tresult PLUGIN_API tell(SB::int64*out)override{if(!out)return SB::kInvalidArgument;*out=position;return 0;}
};
class Events:public V::IEventList{
public:
    std::vector<V::Event>events;int count=0;STATIC_INTERFACE(V::IEventList,V::IEventList_iid)
    SB::int32 PLUGIN_API getEventCount()override{return count;}
    SB::tresult PLUGIN_API getEvent(SB::int32 i,V::Event&e)override{if(i<0||i>=count)return SB::kInvalidArgument;e=events[i];return 0;}
    SB::tresult PLUGIN_API addEvent(V::Event&e)override{if(count>=(int)events.size())return SB::kResultFalse;events[count++]=e;return 0;}
};
class ParameterQueue:public V::IParamValueQueue{
public:
    V::ParamID id=0;std::array<std::pair<int,double>,128>points{};int count=0;STATIC_INTERFACE(V::IParamValueQueue,V::IParamValueQueue_iid)
    V::ParamID PLUGIN_API getParameterId()override{return id;}
    SB::int32 PLUGIN_API getPointCount()override{return count;}
    SB::tresult PLUGIN_API getPoint(SB::int32 i,SB::int32&offset,V::ParamValue&v)override{if(i<0||i>=count)return SB::kInvalidArgument;offset=points[i].first;v=points[i].second;return 0;}
    SB::tresult PLUGIN_API addPoint(SB::int32 offset,V::ParamValue v,SB::int32&i)override{for(i=0;i<count;i++)if(points[i].first==offset){points[i].second=v;return 0;}if(count>=128)return SB::kResultFalse;i=count++;points[i]={offset,v};return 0;}
};
class Parameters:public V::IParameterChanges{
public:
    std::vector<ParameterQueue>queues;int count=0;STATIC_INTERFACE(V::IParameterChanges,V::IParameterChanges_iid)
    SB::int32 PLUGIN_API getParameterCount()override{return count;}
    V::IParamValueQueue* PLUGIN_API getParameterData(SB::int32 i)override{return i>=0&&i<count?&queues[i]:nullptr;}
    V::IParamValueQueue* PLUGIN_API addParameterData(const V::ParamID&id,SB::int32&i)override{for(i=0;i<count;i++)if(queues[i].id==id)return &queues[i];if(count>=(int)queues.size())return nullptr;i=count++;queues[i].id=id;queues[i].count=0;return &queues[i];}
};
struct ParamEdit{V::ParamID id;double value;};
struct Plugin::Impl:public V::IHostApplication,public V::IComponentHandler,public SB::IPlugFrame{
    std::unique_ptr<AraHost>ara;
    HMODULE module=nullptr,sharedCore=nullptr;bool moduleInitialized=false;SB::IPluginFactory*factory=nullptr;V::IComponent*component=nullptr;V::IAudioProcessor*processor=nullptr;V::IEditController*controller=nullptr;
    V::IConnectionPoint*componentPoint=nullptr,*controllerPoint=nullptr;SB::IPlugView*view=nullptr;HWND window=nullptr;
    bool componentInitialized=false,controllerInitialized=false,active=false,isInstrument=false,noteInput=false;
    int latency=0;double rate=48000;float editorScale=1;
    PluginAudioBuses buses;
    Events events,outEvents;Parameters params,outParams;Spsc<ParamEdit,1024> edits;std::atomic<int>restartFlags{0};std::atomic<bool>edited{false};
    std::array<std::array<V::ParamID,130>,16>midiMap{};std::array<uint32_t,1024>midi{};int midiCount=0;
    void refreshMidiMap(){for(auto&channel:midiMap)channel.fill(V::kNoParamId);if(!controller)return;V::IMidiMapping*mapping=nullptr;if(controller->queryInterface(V::IMidiMapping_iid,(void**)&mapping)!=0)return;for(int channel=0;channel<16;channel++)for(int cc=0;cc<130;cc++){V::ParamID id=V::kNoParamId;if(mapping->getMidiControllerAssignment(0,channel,(V::CtrlNumber)cc,id)==0)midiMap[channel][cc]=id;}mapping->release();}
    void openModule(const fs::path&source){
        fs::path binary=fs::absolute(source);if(fs::is_directory(binary))for(auto&e:fs::directory_iterator(binary/L"Contents/x86_64-win"))if(e.path().extension()==L".vst3"){binary=e.path();break;}
        // RX's small VST3 hook aborts in GetPluginFactory if its shared audio
        // engine cannot be loaded. Resolve it first and keep it alive until
        // after the hook unloads, so a broken installation gives an error.
        if(binary.stem().wstring().find(L"RX 11 ")==0){
            const wchar_t*core=L"iZRX11Core.dll";auto sibling=binary.parent_path()/core;
            if(fs::exists(sibling))sharedCore=LoadLibraryExW(sibling.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if(!sharedCore)sharedCore=LoadLibraryExW(core,nullptr,LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if(!sharedCore)throw std::runtime_error("RX 11: Windows cannot load iZRX11Core.dll (error "+std::to_string(GetLastError())+"). Repair the RX installation, including its shared audio engine and dependencies.");
        }
        pluginTrace("loadLibrary: "+utf8(binary.wstring()));module=LoadLibraryExW(binary.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!module)throw std::runtime_error("Cannot load 64-bit VST3 module (Windows error "+std::to_string(GetLastError())+")");
        pluginTrace("InitDll");auto init=(bool(PLUGIN_API*)())GetProcAddress(module,"InitDll");if(init&&!init())throw std::runtime_error("VST3 InitDll failed");moduleInitialized=true;
        pluginTrace("GetPluginFactory");auto getFactory=(SB::IPluginFactory*(PLUGIN_API*)())GetProcAddress(module,"GetPluginFactory");if(!getFactory||!(factory=getFactory()))throw std::runtime_error("VST3 factory missing");
    }
    SB::tresult PLUGIN_API queryInterface(const SB::TUID id,void**obj)override{
        if(equalId(id,V::IHostApplication_iid)||equalId(id,SB::FUnknown_iid))*obj=static_cast<V::IHostApplication*>(this);
        else if(equalId(id,V::IComponentHandler_iid))*obj=static_cast<V::IComponentHandler*>(this);
        else if(equalId(id,SB::IPlugFrame_iid))*obj=static_cast<SB::IPlugFrame*>(this);else{*obj=nullptr;return SB::kNoInterface;}return 0;}
    SB::uint32 PLUGIN_API addRef()override{return 1;}SB::uint32 PLUGIN_API release()override{return 1;}
    SB::tresult PLUGIN_API getName(V::String128 name)override{const char16_t*s=pluginHostName16;memset(name,0,256);size_t n=0;while(s[n]&&n<127)n++;memcpy(name,s,n*2);return 0;}
    SB::tresult PLUGIN_API createInstance(SB::TUID cid,SB::TUID iid,void**obj)override{SB::FUnknown*p=nullptr;if(equalId(cid,V::IMessage_iid))p=new Message;else if(equalId(cid,V::IAttributeList_iid))p=new Attributes;if(!p){*obj=nullptr;return SB::kNoInterface;}auto result=p->queryInterface(iid,obj);p->release();return result;}
    SB::tresult PLUGIN_API beginEdit(V::ParamID)override{return 0;}
    SB::tresult PLUGIN_API performEdit(V::ParamID id,V::ParamValue value)override{edited=true;return edits.push({id,value})?0:SB::kResultFalse;}
    SB::tresult PLUGIN_API endEdit(V::ParamID)override{return 0;}
    SB::tresult PLUGIN_API restartComponent(SB::int32 flags)override{restartFlags.fetch_or(flags);return 0;}
    SB::tresult PLUGIN_API resizeView(SB::IPlugView*v,SB::ViewRect*r)override{if(!window||!r)return SB::kResultFalse;RECT rect{0,0,r->right-r->left,r->bottom-r->top};AdjustWindowRect(&rect,GetWindowLongW(window,GWL_STYLE),FALSE);SetWindowPos(window,nullptr,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER);return v->onSize(r);}
    static LRESULT CALLBACK editorChildProc(HWND child,UINT message,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR owner){auto*self=(Impl*)owner;if(message==WM_KEYDOWN&&w==VK_ESCAPE){closePanelWindow(self->window);return 0;}if(message==WM_NCDESTROY)RemoveWindowSubclass(child,editorChildProc,id);return DefSubclassProc(child,message,w,l);}
    static LRESULT CALLBACK windowProc(HWND h,UINT m,WPARAM w,LPARAM l){auto*self=(Impl*)GetWindowLongPtrW(h,GWLP_USERDATA);if(m==WM_NCCREATE){self=(Impl*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)self);}if(self&&m==WM_KEYDOWN&&w==VK_ESCAPE){closePanelWindow(h);return 0;}if(self&&m==WM_CLOSE){closePanelWindow(h);return 0;}return DefWindowProcW(h,m,w,l);}
    ~Impl(){if(view){view->removed();view->setFrame(nullptr);view->release();}if(window)DestroyWindow(window);
        if(active){processor->setProcessing(false);component->setActive(false);}if(ara)ara->close();if(componentPoint&&controllerPoint){componentPoint->disconnect(controllerPoint);controllerPoint->disconnect(componentPoint);}
        if(componentPoint)componentPoint->release();if(controllerPoint)controllerPoint->release();
        if(controller){controller->setComponentHandler(nullptr);if(controllerInitialized)controller->terminate();controller->release();}
        if(processor)processor->release();if(component){if(componentInitialized)component->terminate();component->release();}ara.reset();if(factory)factory->release();
        if(module){auto exit=(bool(PLUGIN_API*)())GetProcAddress(module,"ExitDll");if(moduleInitialized&&exit)exit();FreeLibrary(module);}if(sharedCore)FreeLibrary(sharedCore);}
};
Plugin::Plugin()=default;Plugin::~Plugin()=default;
bool Plugin::load(const fs::path&source,double rate,std::string&error,const AraTrackContext*araContext){
    if(araContext&&!AraEnabled){error="ARA is not enabled in this build";return false;}
    native.reset();impl.reset();vst2.reset();error.clear();auto extension=source.extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);if(extension==L".dll"){vst2=std::make_unique<Vst2Host>();path=source;if(!vst2->load(source,rate,error)){vst2.reset();return false;}info=vst2->info;return true;}int nativeIndex=nativeEffectIndex(source);if(nativeIndex>=0){native=std::make_unique<NativeEffect>(nativeIndex,rate);auto&s=native->spec();info={s.name,"Karsei",s.category,false};path=source;return true;}
    try{impl=std::make_unique<Impl>();auto&h=*impl;h.rate=rate;path=source;h.openModule(source);
        SB::IPluginFactory3*f3=nullptr;if(h.factory->queryInterface(SB::IPluginFactory3_iid,(void**)&f3)==0){f3->setHostContext(static_cast<V::IHostApplication*>(&h));f3->release();}
        pluginTrace("createComponent");SB::PFactoryInfo fi{};h.factory->getFactoryInfo(&fi);info.vendor=fi.vendor;bool found=false;
        for(int i=0;i<h.factory->countClasses();i++){SB::PClassInfo ci{};if(h.factory->getClassInfo(i,&ci)!=0||strcmp(ci.category,kVstAudioEffectClass))continue;
            if(h.factory->createInstance(ci.cid,V::IComponent_iid,(void**)&h.component)!=0)continue;info.name=ci.name;SB::IPluginFactory2*f2=nullptr;
            if(h.factory->queryInterface(SB::IPluginFactory2_iid,(void**)&f2)==0){SB::PClassInfo2 ci2{};if(f2->getClassInfo2(i,&ci2)==0){info.category=ci2.subCategories;info.instrument=info.category.find("Instrument")!=std::string::npos;}f2->release();}found=true;break;}
        if(!found)throw std::runtime_error("No VST3 audio processor class");h.component->setIoMode(V::kAdvanced);
        pluginTrace("initializeComponent");if(h.component->initialize(static_cast<V::IHostApplication*>(&h))!=0)throw std::runtime_error("VST3 initialize failed");h.componentInitialized=true;
        if(h.component->queryInterface(V::IAudioProcessor_iid,(void**)&h.processor)!=0)throw std::runtime_error("VST3 audio interface missing");
        if(h.processor->canProcessSampleSize(V::kSample32)!=0)throw std::runtime_error("Plugin requires 64-bit processing; this build hosts float32");
        if(araContext){h.ara=std::make_unique<AraHost>(static_cast<V::IComponent*>(h.component),*araContext);info.araAvailable=true;}
        pluginTrace("createController");if(h.component->queryInterface(V::IEditController_iid,(void**)&h.controller)!=0){SB::TUID cid;if(h.component->getControllerClassId(cid)==0&&h.factory->createInstance(cid,V::IEditController_iid,(void**)&h.controller)==0){
                if(h.controller->initialize(static_cast<V::IHostApplication*>(&h))!=0)throw std::runtime_error("VST3 controller initialize failed");h.controllerInitialized=true;}}
        pluginTrace("connectController");if(h.controller){h.controller->setComponentHandler(&h);h.component->queryInterface(V::IConnectionPoint_iid,(void**)&h.componentPoint);h.controller->queryInterface(V::IConnectionPoint_iid,(void**)&h.controllerPoint);
            if(h.componentPoint&&h.controllerPoint){h.componentPoint->connect(h.controllerPoint);h.controllerPoint->connect(h.componentPoint);}MemoryStream state;if(h.component->getState(&state)==0){state.position=0;h.controller->setComponentState(&state);}}
        h.refreshMidiMap();
        pluginTrace("configureBuses");h.buses.configure(*h.component,*h.processor);
        h.noteInput=h.component->getBusCount(V::kEvent,V::kInput)>0;if(h.noteInput)h.events.events.resize(MaxBlockEvents);if(h.component->getBusCount(V::kEvent,V::kOutput)>0)h.outEvents.events.resize(512);int parameterQueues=h.controller?(int)clamp(h.controller->getParameterCount(),16,256):64;h.params.queues.resize(parameterQueues);h.outParams.queues.resize(parameterQueues);
        h.isInstrument=!h.ara&&(info.instrument||h.buses.mainInput<0);info.instrument=h.isInstrument;
        pluginTrace("setupProcessing");V::ProcessSetup setup{V::kRealtime,V::kSample32,MaxBlock,rate};if(h.processor->setupProcessing(setup)!=0)throw std::runtime_error("VST3 setupProcessing failed");
        h.buses.activate(*h.component);if(h.noteInput)h.component->activateBus(V::kEvent,V::kInput,0,true);if(h.component->getBusCount(V::kEvent,V::kOutput)>0)h.component->activateBus(V::kEvent,V::kOutput,0,true);
        h.latency=h.processor->getLatencySamples();if(h.latency>131071)throw std::runtime_error("Plugin latency exceeds alpha limit (131071 samples)");
        pluginTrace("setActive");if(h.component->setActive(true)!=0)throw std::runtime_error("VST3 activation failed");h.active=true;auto processing=h.processor->setProcessing(true);pluginTrace("setProcessing result: "+std::to_string(processing));if(!processingNotificationAccepted(processing))throw std::runtime_error("VST3 processing activation failed (result "+std::to_string(processing)+")");if(h.ara)info.name+=" [ARA]";pluginTrace("loaded");return true;
    }catch(const std::exception&e){error=e.what();pluginTrace("error: "+error);impl.reset();return false;}}
void Plugin::process(float*l,float*r,int frames,const PluginNote*notes,int count,double beat,double tempo,bool playing,const std::vector<Point>*automation,uint32_t parameter,const float*sideL,const float*sideR,const std::vector<AutomationLane>*lanes,double loopLength,double loopStart){
    if(vst2){vst2->process(l,r,frames,notes,count,beat,tempo,playing,automation,parameter,sideL,sideR,lanes,loopLength,loopStart);return;}
    if(native){native->process(l,r,frames,beat,tempo,playing,automation,parameter,sideL,sideR,lanes,loopLength,loopStart);return;}
    if(!impl||!impl->active)return;auto&h=*impl;h.events.count=0;h.outEvents.count=0;h.params.count=h.outParams.count=0;ParamEdit edit;
    while(h.edits.pop(edit)){SB::int32 i=0;auto*q=h.params.addParameterData(edit.id,i);if(q)q->addPoint(0,edit.value,i);}
    for(int i=0;i<h.midiCount;i++){auto m=h.midi[i];int status=m&0xf0,ch=m&15,n=(m>>8)&127,v=(m>>16)&127;int cc=status==0xb0?n:status==0xd0?128:status==0xe0?129:-1;double value=status==0xe0?(n+(v<<7))/16383.:status==0xd0?n/127.:v/127.;if(cc>=0&&h.midiMap[ch][cc]!=V::kNoParamId){SB::int32 index=0;auto*q=h.params.addParameterData(h.midiMap[ch][cc],index);if(q)q->addPoint(0,value,index);}else if(status==0xa0&&h.noteInput){V::Event e{};e.type=V::Event::kPolyPressureEvent;e.busIndex=0;e.sampleOffset=0;e.ppqPosition=beat;e.polyPressure.channel=ch;e.polyPressure.pitch=n;e.polyPressure.pressure=v/127.f;e.polyPressure.noteId=-1;h.events.addEvent(e);}}h.midiCount=0;
    auto addLine=[&](uint32_t id,const std::vector<Point>&points){SB::int32 index=0;auto*q=h.params.addParameterData(id,index);if(q)visitAutomationBlock(points,beat,playing?tempo/(60*h.rate):0,frames,loopLength,[&](int offset,double value){q->addPoint(offset,value,index);},loopStart);};if(automation)addLine(parameter,*automation);if(lanes)for(auto&lane:*lanes)if(lane.enabled)addLine(lane.parameter,lane.points);
    for(int i=0;i<count;i++){auto&n=notes[i];V::Event e{};e.busIndex=0;e.sampleOffset=n.offset;e.ppqPosition=beat+n.offset/h.rate*tempo/60.;if(loopLength>0&&e.ppqPosition>=loopLength)e.ppqPosition=loopStart+std::fmod(e.ppqPosition-loopLength,loopLength-loopStart);e.type=n.on?V::Event::kNoteOnEvent:V::Event::kNoteOffEvent;
        if(n.on){e.noteOn.channel=n.channel;e.noteOn.pitch=n.pitch;e.noteOn.velocity=n.velocity;e.noteOn.noteId=n.id;}else{e.noteOff.channel=n.channel;e.noteOff.pitch=n.pitch;e.noteOff.velocity=0;e.noteOff.noteId=n.id;}h.events.addEvent(e);}
    // ARA playback owns the Playlist source. Feeding the dry clip as well would double it
    // in plug-ins that also expose an ordinary VST input path.
    if(h.ara){std::fill_n(l,frames,0.f);std::fill_n(r,frames,0.f);}
    h.buses.beginBlock(l,r,frames,sideL,sideR);
    V::ProcessContext context{};context.state=V::ProcessContext::kTempoValid|V::ProcessContext::kTimeSigValid|V::ProcessContext::kProjectTimeMusicValid|V::ProcessContext::kBarPositionValid|(playing?V::ProcessContext::kPlaying:0);
    if(loopLength>0){context.state|=V::ProcessContext::kCycleActive|V::ProcessContext::kCycleValid;context.cycleStartMusic=loopStart;context.cycleEndMusic=loopLength;}
    context.sampleRate=h.rate;context.projectTimeSamples=h.ara?(SB::int64)std::llround(h.ara->clock().seconds(beat)*h.rate):(SB::int64)(beat*60./tempo*h.rate);context.projectTimeMusic=beat;context.barPositionMusic=std::floor(beat/4)*4;context.tempo=tempo;context.timeSigNumerator=context.timeSigDenominator=4;if(h.ara){auto meter=h.ara->clock().meterAt(beat);context.timeSigNumerator=meter.numerator;context.timeSigDenominator=meter.denominator;context.barPositionMusic=h.ara->clock().barStart(beat);}
    V::ProcessData data;data.processMode=V::kRealtime;data.symbolicSampleSize=V::kSample32;data.numSamples=frames;data.numInputs=(int)h.buses.inputs.size();data.numOutputs=(int)h.buses.outputs.size();data.inputs=h.buses.inputs.empty()?nullptr:h.buses.inputs.data();data.outputs=h.buses.outputs.data();
    data.inputEvents=&h.events;data.outputEvents=&h.outEvents;data.inputParameterChanges=&h.params;data.outputParameterChanges=&h.outParams;data.processContext=&context;
    if(h.processor->process(data)==0)h.buses.copyOutput(l,r,frames);
}
void Plugin::hideEditor(){if(vst2)vst2->hideEditor();if(native&&native->window)ShowWindow(native->window,SW_HIDE);if(impl&&impl->window)ShowWindow(impl->window,SW_HIDE);}
void Plugin::editor(HWND parent){if(vst2){vst2->editor(parent);return;}if(native){native->editor(parent);return;}if(!impl||!impl->controller)return;auto&h=*impl;if(h.window){ShowWindow(h.window,SW_SHOW);SetForegroundWindow(h.window);return;}
    if(h.view){h.view->release();h.view=nullptr;}h.view=h.controller->createView(V::ViewType::kEditor);if(!h.view||h.view->isPlatformTypeSupported(SB::kPlatformTypeHWND)!=0){MessageBoxW(parent,L"This plugin has no native Windows editor.",L"VampDAW / VST3",MB_OK);return;}
    static bool registered=false;if(!registered){WNDCLASSW c{};c.lpfnWndProc=Impl::windowProc;c.hInstance=pluginUiModule();c.lpszClassName=hostedVst3Class;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&c);registered=true;}
    SB::ViewRect size{};h.view->getSize(&size);RECT rect{0,0,std::max(300,size.right-size.left),std::max(200,size.bottom-size.top)};AdjustWindowRect(&rect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE);
    h.window=CreateWindowExW(0,hostedVst3Class,wide(info.name+" / VampDAW").c_str(),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,parent,nullptr,pluginUiModule(),&h);
    h.view->setFrame(&h);if(h.view->attached(h.window,SB::kPlatformTypeHWND)!=0){DestroyWindow(h.window);h.window=nullptr;return;}EnumChildWindows(h.window,[](HWND child,LPARAM owner)->BOOL{SetWindowSubclass(child,Impl::editorChildProc,0x56414d50,(DWORD_PTR)owner);return TRUE;},(LPARAM)&h);ShowWindow(h.window,SW_SHOW);}
Json Plugin::state(){if(vst2)return vst2->state();if(native)return native->state();if(!impl)return {};auto&h=*impl;MemoryStream component,controller;Json j;if(h.component->getState(&component)==0)j["component"]=encodePluginStateBytes(component.bytes);if(h.controller&&h.controller->getState(&controller)==0)j["controller"]=encodePluginStateBytes(controller.bytes);if(h.ara)j["ara"]=h.ara->state();return j;}
void Plugin::restore(const Json&j){if(vst2){vst2->restore(j);return;}if(native){native->restore(j);return;}if(!impl||!j.is_object())return;auto&h=*impl;if(j.contains("ara")&&!h.ara)throw std::runtime_error("ARA edits require an ARA-bound audio track; they were not applied");if(h.active){h.processor->setProcessing(false);h.component->setActive(false);}h.active=false;
    if(j.contains("component")){MemoryStream s;s.bytes=decodePluginStateBytes(j["component"]);h.component->setState(&s);if(h.controller){s.position=0;h.controller->setComponentState(&s);}}
    if(j.contains("controller")&&h.controller){MemoryStream s;s.bytes=decodePluginStateBytes(j["controller"]);h.controller->setState(&s);}
    if(h.ara&&j.contains("ara"))h.ara->restore(j["ara"]);
    // Restored presets/projects can change bus counts and channel arrangements.
    prepare(h.rate);}
int Plugin::latency()const{if(vst2)return vst2->latency();if(native)return native->latency();return impl?impl->latency:0;}bool Plugin::instrument()const{return vst2?info.instrument:impl&&impl->isInstrument;}
bool Plugin::acceptsNotes()const{if(vst2)return vst2->acceptsNotes();return impl&&impl->noteInput;}
Json Plugin::audioBusLayout()const{if(vst2)return vst2->buses();if(native)return Json{{"inputs",{2,2}},{"outputs",{2}},{"main_input",0},{"sidechain_input",1},{"main_output",0}};return impl?impl->buses.description():Json();}
bool Plugin::takeChanges(){if(vst2)return vst2->takeChanges();if(native)return native->edited.exchange(false);return impl&&impl->edited.exchange(false);}
std::vector<PluginParameter>Plugin::parameters(){if(vst2)return vst2->parameters();std::vector<PluginParameter>result;if(native){for(size_t i=0;i<native->spec().parameters.size();i++)result.push_back({(uint32_t)i,native->spec().parameters[i].name,native->spec().parameters[i].unit});return result;}if(!impl||!impl->controller)return result;auto*c=impl->controller;
    for(int i=0;i<c->getParameterCount();i++){V::ParameterInfo p{};if(c->getParameterInfo(i,p)==0&&(p.flags&V::ParameterInfo::kCanAutomate)&&!(p.flags&V::ParameterInfo::kIsReadOnly))result.push_back({p.id,utf8(std::wstring((const wchar_t*)p.title)),utf8(std::wstring((const wchar_t*)p.units))});}return result;}
void Plugin::displayParameter(uint32_t id,double value){if(vst2)return;if(native){native->set(id,value);return;}if(impl&&impl->controller)impl->controller->setParamNormalized(id,value);}
bool Plugin::setParameter(uint32_t id,double value){if(vst2)return vst2->setParameter(id,value);if(!std::isfinite(value))return false;value=clamp(value,0,1);if(native){if(id>=native->spec().parameters.size())return false;native->set(id,value,true);return true;}if(!impl||!impl->controller||!impl->edits.push({id,value}))return false;impl->controller->setParamNormalized(id,value);impl->edited=true;return true;}
double Plugin::parameterValue(uint32_t id)const{if(vst2)return vst2->parameterValue(id);if(native)return id<native->spec().parameters.size()?native->values[id].load():0;return impl&&impl->controller?impl->controller->getParamNormalized(id):0;}
bool Plugin::setParameterText(uint32_t id,const std::string&text){if(vst2)return false;if(native){if(id>=native->spec().parameters.size())return false;try{size_t end=0;double real=std::stod(text,&end);if(end!=text.size()||!std::isfinite(real))return false;return setParameter(id,native->spec().parameters[id].normalized(real));}catch(...){return false;}}if(!impl||!impl->controller)return false;auto value=wide(text);if(value.size()>127)return false;V::String128 input{};memcpy(input,value.data(),value.size()*2);double normalized=0;if(impl->controller->getParamValueByString(id,input,normalized)!=0||!std::isfinite(normalized)||normalized<0||normalized>1)return false;return setParameter(id,normalized);}
std::string Plugin::parameterDisplay(uint32_t id,double normalized){if(vst2)return vst2->parameterDisplay(id,normalized);if(native){if(id>=native->spec().parameters.size())return{};return std::to_string(native->spec().parameters[id].value(normalized));}if(!impl||!impl->controller)return{};V::String128 text{};if(impl->controller->getParamStringByValue(id,normalized,text)!=0)return{};return utf8(std::wstring((wchar_t*)text));}
bool Plugin::setParameterPhysical(uint32_t id,double value,const std::string&unit){
    if(!std::isfinite(value))return false;auto all=parameters();auto p=std::find_if(all.begin(),all.end(),[&](auto&p){return p.id==id;});if(p==all.end())return false;auto actual=physicalUnit(p->unit),expected=physicalUnit(unit);if(actual.empty()){auto text=parameterDisplay(id,.5);size_t at=text.find_first_of("-+.0123456789");if(at!=std::string::npos){char*end=nullptr;std::strtod(text.c_str()+at,&end);if(end&&end!=text.c_str()+at)actual=physicalUnit(end);}}
    if(expected.empty()&&!actual.empty()&&actual!="1"&&actual!="ratio"&&actual!="q")return false;
    if(expected=="db"&&actual!="db"&&actual!="dbfs")return false;if(expected=="ms"&&actual!="ms"&&actual!="s"&&actual!="sec")return false;if(expected=="hz"&&actual!="hz"&&actual!="khz")return false;if(expected=="%"&&actual!="%"&&actual!="percent")return false;
    auto read=[&](double n){auto text=parameterDisplay(id,n);auto parsed=physicalReadout(text,unit);if(parsed){if(expected=="hz"&&actual=="khz"&&physicalUnit(text).find("khz")==std::string::npos)*parsed*=1000;if(expected=="ms"&&(actual=="s"||actual=="sec")&&physicalUnit(text).find("s")==std::string::npos)*parsed*=1000;}return parsed;};
    double lower=0,upper=1;auto low=read(lower),high=read(upper);for(double epsilon:{.000001,.0001,.001,.01}){if(!low){lower=epsilon;low=read(lower);}if(!high){upper=1-epsilon;high=read(upper);}}if(!low||!high||std::abs(*high-*low)<1e-9)return false;if(expected.empty()&&value>1.0001&&std::max(*low,*high)<=1.0001)return false;double target=clamp(value,std::min(*low,*high),std::max(*low,*high));bool rising=*high>*low;
    // Check the readout is monotonic before deriving any normalized value.
    double previous=*low;for(double n:{.25,.5,.75,1.}){auto v=read(lower+(upper-lower)*n);if(!v||(rising?*v+1e-7<previous:*v-1e-7>previous))return false;previous=*v;}
    double a=lower,b=upper,best=lower,error=1e100;for(int i=0;i<24;i++){double n=(a+b)*.5;auto v=read(n);if(!v)return false;double e=std::abs(*v-target);if(e<error){best=n;error=e;}if((*v<target)==rising)a=n;else b=n;}
    if(error>std::max(.05,std::abs(target)*.025))return false;return setParameter(id,best);
}
void Plugin::midiMessage(uint32_t message){if(vst2){vst2->midiMessage(message);return;}if(impl&&impl->midiCount<(int)impl->midi.size())impl->midi[impl->midiCount++]=message;}
bool Plugin::needsRestart(){if(vst2)return vst2->needsRestart();return impl&&(impl->restartFlags.exchange(0)&(V::kReloadComponent|V::kIoChanged|V::kLatencyChanged|V::kMidiCCAssignmentChanged));}
bool Plugin::prepare(double rate){if(vst2)return vst2->prepare(rate);if(native)return native->prepare(rate);if(!impl)return true;auto&h=*impl;h.refreshMidiMap();if(h.active){h.processor->setProcessing(false);h.component->setActive(false);}h.active=false;
    try{h.buses.configure(*h.component,*h.processor);}catch(const std::exception&){return false;}
    V::ProcessSetup setup{V::kRealtime,V::kSample32,MaxBlock,rate};if(h.processor->setupProcessing(setup)!=0)return false;h.latency=h.processor->getLatencySamples();if(h.latency>131071)return false;
    h.buses.activate(*h.component);if(h.noteInput)h.component->activateBus(V::kEvent,V::kInput,0,true);if(h.component->getBusCount(V::kEvent,V::kOutput)>0)h.component->activateBus(V::kEvent,V::kOutput,0,true);
    h.rate=rate;if(h.component->setActive(true)!=0)return false;h.active=true;if(!processingNotificationAccepted(h.processor->setProcessing(true))){h.component->setActive(false);h.active=false;return false;}return true;}
bool Plugin::araActive()const{return impl&&impl->ara;}
bool Plugin::araNeedsUpdate(const AraTrackContext&context)const{return impl&&impl->ara&&impl->ara->needsUpdate(context);}
void Plugin::syncAra(const AraTrackContext&context){if(!araNeedsUpdate(context))return;auto&h=*impl;bool wasActive=h.active;if(wasActive){h.processor->setProcessing(false);h.component->setActive(false);h.active=false;}try{h.ara->sync(context);}catch(...){if(wasActive)prepare(h.rate);throw;}if(wasActive&&!prepare(h.rate))throw std::runtime_error("ARA could not resume processing");}
AraTransportRequest Plugin::pollAra(){if(!araActive())return{};impl->ara->poll();if(impl->ara->takeChanges())impl->edited=true;return impl->ara->takeTransport();}
double Plugin::araProgress()const{return araActive()?impl->ara->progress():-1;}
void Plugin::selectAraClip(const std::string&id){if(araActive())impl->ara->select(id);}
Json describePlugin(const fs::path&source){auto extension=source.extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);if(extension==L".dll"){Plugin p;std::string error;if(!p.load(source,48000,error))throw std::runtime_error(error);return Json{{"path",utf8(source.wstring())},{"name",p.info.name},{"vendor",p.info.vendor},{"category",p.info.instrument?"Instrument":"Fx"},{"format","VST2"},{"instrument",p.instrument()},{"ok",true}};}int index=nativeEffectIndex(source);if(index>=0)return nativePluginCatalog()[index];Plugin owner;owner.impl=std::make_unique<Plugin::Impl>();auto&h=*owner.impl;h.openModule(source);SB::PFactoryInfo vendor{};h.factory->getFactoryInfo(&vendor);for(int i=0;i<h.factory->countClasses();i++){SB::PClassInfo ci{};if(h.factory->getClassInfo(i,&ci)!=0||strcmp(ci.category,kVstAudioEffectClass))continue;SB::IPluginFactory2*f2=nullptr;std::string categories;if(h.factory->queryInterface(SB::IPluginFactory2_iid,(void**)&f2)==0){SB::PClassInfo2 c2{};if(f2->getClassInfo2(i,&c2)==0)categories=c2.subCategories;f2->release();}bool ara=false;for(int n=0;n<h.factory->countClasses();n++){SB::PClassInfo a{};if(h.factory->getClassInfo(n,&a)==0&&!strcmp(a.category,kARAMainFactoryClass)&&!strcmp(a.name,ci.name))ara=true;}return Json{{"ara",ara},{"path",utf8(source.wstring())},{"name",ci.name},{"vendor",vendor.vendor},{"category",categories},{"instrument",categories.find("Instrument")!=std::string::npos},{"ok",true}};}throw std::runtime_error("No audio class");}
std::vector<fs::path>findPlugins(const std::vector<fs::path>&roots){std::vector<fs::path>result;std::set<std::wstring>seen;auto key=[](const fs::path&p){auto k=p.lexically_normal().wstring();std::transform(k.begin(),k.end(),k.begin(),::towlower);return k;};for(auto&root:roots){std::error_code ec;if(!fs::is_directory(root,ec))continue;
    for(fs::recursive_directory_iterator i(root,fs::directory_options::skip_permission_denied,ec),end;i!=end;i.increment(ec)){if(ec){ec.clear();continue;}auto extension=i->path().extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);if(extension==L".vst3"||extension==L".dll"){if(extension==L".vst3"||(!i->is_directory(ec)&&vst2Candidate(i->path()))){auto canonical=fs::weakly_canonical(i->path(),ec);if(ec){ec.clear();canonical=i->path();}if(seen.insert(key(canonical)).second)result.push_back(i->path());}if(i->is_directory(ec))i.disable_recursion_pending();}}}std::sort(result.begin(),result.end());return result;}
}
