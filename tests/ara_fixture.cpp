#define INIT_CLASS_IID
#define VAMP_FACTORY_FIXTURE
#include "engine.hpp"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "plugin_bus_tests.hpp"
#include "ara/ARAVST3.h"
#include <set>
#include <cstring>
using namespace vamp;using namespace ARA;namespace SB=Steinberg;namespace V=Steinberg::Vst;
namespace {
std::array<uint64_t,24>stats{};
void expect(bool ok){if(!ok)stats[0]++;}
struct Source{ARAAudioSourceHostRef host;std::string id;ARAAudioSourceProperties props;bool enabled=false;std::vector<float>left,right;};
struct Modification{Source*source;std::string id;double gain=1;};
struct Region{Modification*mod;ARAPlaybackRegionProperties props;};
struct Document{ARADocumentControllerHostInstance host;ARADocumentControllerInstance instance;bool editing=false,dirty=false,transport=false;std::set<Source*>sources;std::set<Modification*>mods;std::set<Region*>regions;std::set<Region*>playback;std::set<Region*>editor;void*context=nullptr;void*sequence=nullptr;};
std::set<Document*>documents;
const ARAFactory*araFactory();
Document&doc(ARADocumentControllerRef r){return*(Document*)r;}
void edit(Document&d){expect(d.editing);}
ARADocumentControllerInterface makeDocumentInterface(){ARADocumentControllerInterface api{};api.structSize=sizeof api;
    api.destroyDocumentController=[](ARADocumentControllerRef r){auto&d=doc(r);expect(!d.editing&&d.regions.empty()&&d.mods.empty()&&d.sources.empty()&&!d.context&&!d.sequence&&d.playback.empty()&&d.editor.empty());documents.erase(&d);delete &d;stats[4]++;stats[19]--;};
    api.getFactory=[](ARADocumentControllerRef){return araFactory();};
    api.beginEditing=[](ARADocumentControllerRef r){auto&d=doc(r);expect(!d.editing);d.editing=true;};
    api.endEditing=[](ARADocumentControllerRef r){auto&d=doc(r);edit(d);d.editing=false;};
    api.notifyModelUpdates=[](ARADocumentControllerRef r){auto&d=doc(r);expect(!d.editing);auto&h=d.host;if(d.dirty){h.modelUpdateControllerInterface->notifyDocumentDataChanged(h.modelUpdateControllerHostRef);d.dirty=false;}for(auto*s:d.sources)if(s->enabled){h.modelUpdateControllerInterface->notifyAudioSourceAnalysisProgress(h.modelUpdateControllerHostRef,s->host,kARAAnalysisProgressStarted,0);h.modelUpdateControllerInterface->notifyAudioSourceAnalysisProgress(h.modelUpdateControllerHostRef,s->host,kARAAnalysisProgressCompleted,1);}if(d.transport){auto*p=h.playbackControllerInterface;p->requestSetPlaybackPosition(h.playbackControllerHostRef,1.25);p->requestSetCycleRange(h.playbackControllerHostRef,.5,2);p->requestEnableCycle(h.playbackControllerHostRef,kARATrue);p->requestStartPlayback(h.playbackControllerHostRef);d.transport=false;}};
    api.updateDocumentProperties=[](ARADocumentControllerRef r,const ARADocumentProperties*p){edit(doc(r));expect(p&&p->name);};
    api.createMusicalContext=[](ARADocumentControllerRef r,ARAMusicalContextHostRef h,const ARAMusicalContextProperties*)->ARAMusicalContextRef{auto&d=doc(r);edit(d);d.context=new int(1);auto&api=*d.host.contentAccessControllerInterface;auto c=d.host.contentAccessControllerHostRef;for(auto type:{kARAContentTypeTempoEntries,kARAContentTypeBarSignatures}){expect(api.isMusicalContextContentAvailable(c,h,type));auto reader=api.createMusicalContextContentReader(c,h,type,nullptr);int count=api.getContentReaderEventCount(c,reader);expect(count>0);for(int n=0;n<count;n++)expect(api.getContentReaderDataForEvent(c,reader,n)!=nullptr);expect(api.getContentReaderDataForEvent(c,reader,-1)==nullptr);api.destroyContentReader(c,reader);stats[type==kARAContentTypeTempoEntries?7:8]++;}return(ARAMusicalContextRef)d.context;};
    api.updateMusicalContextContent=[](ARADocumentControllerRef r,ARAMusicalContextRef,const ARAContentTimeRange*,ARAContentUpdateFlags){edit(doc(r));stats[20]++;};
    api.destroyMusicalContext=[](ARADocumentControllerRef r,ARAMusicalContextRef c){auto&d=doc(r);edit(d);expect(!d.sequence);delete(int*)c;d.context=nullptr;};
    api.createRegionSequence=[](ARADocumentControllerRef r,ARARegionSequenceHostRef,const ARARegionSequenceProperties*p)->ARARegionSequenceRef{auto&d=doc(r);edit(d);expect(p->musicalContextRef==(ARAMusicalContextRef)d.context);d.sequence=new int(2);return(ARARegionSequenceRef)d.sequence;};
    api.updateRegionSequenceProperties=[](ARADocumentControllerRef r,ARARegionSequenceRef,const ARARegionSequenceProperties*){edit(doc(r));};
    api.destroyRegionSequence=[](ARADocumentControllerRef r,ARARegionSequenceRef s){auto&d=doc(r);edit(d);expect(d.regions.empty());delete(int*)s;d.sequence=nullptr;};
    api.createAudioSource=[](ARADocumentControllerRef r,ARAAudioSourceHostRef h,const ARAAudioSourceProperties*p)->ARAAudioSourceRef{auto&d=doc(r);edit(d);expect(p->channelCount==2&&p->sampleCount>0);auto*s=new Source{h,p->persistentID,*p};d.sources.insert(s);return(ARAAudioSourceRef)s;};
    api.updateAudioSourceProperties=[](ARADocumentControllerRef r,ARAAudioSourceRef s,const ARAAudioSourceProperties*p){edit(doc(r));auto&a=*(Source*)s;expect(!a.enabled);a.props=*p;};
    api.updateAudioSourceContent=[](ARADocumentControllerRef r,ARAAudioSourceRef,const ARAContentTimeRange*,ARAContentUpdateFlags){edit(doc(r));stats[15]++;};
    api.enableAudioSourceSamplesAccess=[](ARADocumentControllerRef r,ARAAudioSourceRef ref,ARABool enabled){auto&d=doc(r);auto&s=*(Source*)ref;s.enabled=enabled!=0;if(!enabled){stats[17]++;return;}stats[16]++;auto&api=*d.host.audioAccessControllerInterface;auto c=d.host.audioAccessControllerHostRef;s.left.resize(s.props.sampleCount);s.right.resize(s.props.sampleCount);auto reader=api.createAudioReaderForSource(c,s.host,kARAFalse);void*buffers[]{s.left.data(),s.right.data()};expect(api.readAudioSamples(c,reader,0,s.props.sampleCount,buffers)!=0);stats[5]++;float l[5]{},rr[5]{};void*edges[]{l,rr};expect(api.readAudioSamples(c,reader,-3,5,edges)!=0&&l[0]==0&&rr[2]==0);expect(api.readAudioSamples(c,reader,s.props.sampleCount,5,edges)!=0&&l[0]==0&&rr[4]==0);api.destroyAudioReader(c,reader);reader=api.createAudioReaderForSource(c,s.host,kARATrue);double ll[8]{},dr[8]{};void*wide[]{ll,dr};expect(api.readAudioSamples(c,reader,0,8,wide)!=0);for(int n=0;n<8;n++)expect(std::abs(ll[n]-s.left[n])<1e-10&&std::abs(dr[n]-s.right[n])<1e-10);stats[6]++;api.destroyAudioReader(c,reader);};
    api.destroyAudioSource=[](ARADocumentControllerRef r,ARAAudioSourceRef s){auto&d=doc(r);edit(d);for(auto*m:d.mods)expect(m->source!=(Source*)s);expect(!((Source*)s)->enabled);d.sources.erase((Source*)s);delete(Source*)s;};
    api.createAudioModification=[](ARADocumentControllerRef r,ARAAudioSourceRef s,ARAAudioModificationHostRef,const ARAAudioModificationProperties*p)->ARAAudioModificationRef{auto&d=doc(r);edit(d);auto*m=new Modification{(Source*)s,p->persistentID};d.mods.insert(m);return(ARAAudioModificationRef)m;};
    api.destroyAudioModification=[](ARADocumentControllerRef r,ARAAudioModificationRef m){auto&d=doc(r);edit(d);for(auto*p:d.regions)expect(p->mod!=(Modification*)m);d.mods.erase((Modification*)m);delete(Modification*)m;};
    api.createPlaybackRegion=[](ARADocumentControllerRef r,ARAAudioModificationRef m,ARAPlaybackRegionHostRef,const ARAPlaybackRegionProperties*p)->ARAPlaybackRegionRef{auto&d=doc(r);edit(d);expect(p->regionSequenceRef==(ARARegionSequenceRef)d.sequence);auto*region=new Region{(Modification*)m,*p};d.regions.insert(region);return(ARAPlaybackRegionRef)region;};
    api.updatePlaybackRegionProperties=[](ARADocumentControllerRef r,ARAPlaybackRegionRef region,const ARAPlaybackRegionProperties*p){edit(doc(r));((Region*)region)->props=*p;};
    api.destroyPlaybackRegion=[](ARADocumentControllerRef r,ARAPlaybackRegionRef region){auto&d=doc(r);edit(d);expect(!d.playback.count((Region*)region)&&!d.editor.count((Region*)region));d.regions.erase((Region*)region);delete(Region*)region;};
    api.storeObjectsToArchive=[](ARADocumentControllerRef r,ARAArchiveWriterHostRef a,const ARAStoreObjectsFilter*f)->ARABool{auto&d=doc(r);expect(!d.editing);Json j=Json::object();for(auto*m:d.mods){bool included=!f;for(ARASize n=0;f&&n<f->audioModificationRefsCount;n++)included|=(void*)f->audioModificationRefs[n]==m;if(included)j[m->id]=m->gain;}auto text=j.dump();stats[9]++;return d.host.archivingControllerInterface->writeBytesToArchive(d.host.archivingControllerHostRef,a,0,text.size(),(const ARAByte*)text.data());};
    api.restoreObjectsFromArchive=[](ARADocumentControllerRef r,ARAArchiveReaderHostRef a,const ARARestoreObjectsFilter*f)->ARABool{auto&d=doc(r);edit(d);auto&api=*d.host.archivingControllerInterface;auto h=d.host.archivingControllerHostRef;expect(!strcmp(api.getDocumentArchiveID(h,a),"com.karsei.ara.fixture.archive"));auto size=api.getArchiveSize(h,a);std::string text(size,' ');if(!api.readBytesFromArchive(h,a,0,size,(ARAByte*)text.data()))return kARAFalse;auto j=Json::parse(text);for(auto*m:d.mods){std::string id=m->id;bool included=!f;for(ARASize n=0;f&&n<f->audioModificationIDsCount;n++){auto current=f->audioModificationCurrentIDs?f->audioModificationCurrentIDs[n]:f->audioModificationArchiveIDs[n];if(id==current){id=f->audioModificationArchiveIDs[n];included=true;break;}}if(included&&j.contains(id))m->gain=j[id].get<double>();}stats[10]++;return kARATrue;};
    return api;
}
const ARADocumentControllerInterface documentInterface=makeDocumentInterface();
ARAFactory makeFactory(){ARAFactory f{};f.structSize=sizeof f;f.lowestSupportedApiGeneration=kARAAPIGeneration_2_0_Final;f.highestSupportedApiGeneration=kARAAPIGeneration_2_3_Final;f.factoryID="com.karsei.ara.fixture";f.plugInName="ARA Contract Fixture";f.manufacturerName="Karsei Test";f.informationURL="https://example.invalid";f.version="1.0";f.documentArchiveID="com.karsei.ara.fixture.archive";
    f.initializeARAWithConfiguration=[](const ARAInterfaceConfiguration*c){expect(stats[1]==stats[2]&&c->assertFunctionAddress&&c->desiredApiGeneration>=kARAAPIGeneration_2_0_Final);stats[1]++;};
    f.uninitializeARA=[](){expect(documents.empty());stats[2]++;};
    f.createDocumentControllerWithDocument=[](const ARADocumentControllerHostInstance*h,const ARADocumentProperties*)->const ARADocumentControllerInstance*{expect(stats[1]>stats[2]);auto*d=new Document;d->host=*h;d->instance={sizeof(ARADocumentControllerInstance),(ARADocumentControllerRef)d,&documentInterface};documents.insert(d);stats[3]++;stats[19]++;return &d->instance;};return f;
}
const ARAFactory factory=makeFactory();const ARAFactory*araFactory(){return &factory;}
const ARAPlaybackRendererInterface playbackInterface{sizeof(ARAPlaybackRendererInterface),[](ARAPlaybackRendererRef r,ARAPlaybackRegionRef region){auto&d=*(Document*)r;edit(d);d.playback.insert((Region*)region);stats[13]++;},[](ARAPlaybackRendererRef r,ARAPlaybackRegionRef region){auto&d=*(Document*)r;edit(d);expect(d.playback.erase((Region*)region)==1);stats[14]++;}};
const ARAEditorRendererInterface editorInterface{sizeof(ARAEditorRendererInterface),[](ARAEditorRendererRef r,ARAPlaybackRegionRef region){((Document*)r)->editor.insert((Region*)region);},[](ARAEditorRendererRef r,ARAPlaybackRegionRef region){expect(((Document*)r)->editor.erase((Region*)region)==1);},nullptr,nullptr};
const ARAEditorViewInterface viewInterface{sizeof(ARAEditorViewInterface),[](ARAEditorViewRef r,const ARAViewSelection*s){auto&d=*(Document*)r;expect(s->playbackRegionRefsCount==1&&d.regions.count((Region*)s->playbackRegionRefs[0]));stats[12]++;},nullptr};
class Fixture final:public BusTestPlugin,public IPlugInEntryPoint,public IPlugInEntryPoint2{
    SB::uint32 refs=1;bool active=false,bound=false;Document*d=nullptr;double sampleRate=48000;ARAPlugInExtensionInstance extension{};
public:
    Fixture(){layouts({bus(2)},{bus(2)});}
    SB::tresult PLUGIN_API queryInterface(const SB::TUID iid,void**out)override{*out=nullptr;if(!memcmp(iid,V::IComponent_iid,16)||!memcmp(iid,SB::FUnknown_iid,16))*out=static_cast<V::IComponent*>(this);else if(!memcmp(iid,V::IAudioProcessor_iid,16))*out=static_cast<V::IAudioProcessor*>(this);else if(!memcmp(iid,IPlugInEntryPoint_iid,16))*out=static_cast<IPlugInEntryPoint*>(this);else if(!memcmp(iid,IPlugInEntryPoint2_iid,16))*out=static_cast<IPlugInEntryPoint2*>(this);if(!*out)return SB::kNoInterface;addRef();return 0;}
    SB::uint32 PLUGIN_API addRef()override{return ++refs;}SB::uint32 PLUGIN_API release()override{auto n=--refs;if(!n)delete this;return n;}
    const ARAFactory*PLUGIN_API getFactory()override{return &factory;}
    const ARAPlugInExtensionInstance*PLUGIN_API bindToDocumentController(ARADocumentControllerRef r)override{return bindToDocumentControllerWithRoles(r,7,7);}
    const ARAPlugInExtensionInstance*PLUGIN_API bindToDocumentControllerWithRoles(ARADocumentControllerRef r,ARAPlugInInstanceRoleFlags known,ARAPlugInInstanceRoleFlags assigned)override{expect(!active&&!bound&&known==7&&assigned==7);bound=true;d=&doc(r);extension.structSize=sizeof extension;extension.playbackRendererRef=(ARAPlaybackRendererRef)d;extension.playbackRendererInterface=&playbackInterface;extension.editorRendererRef=(ARAEditorRendererRef)d;extension.editorRendererInterface=&editorInterface;extension.editorViewRef=(ARAEditorViewRef)d;extension.editorViewInterface=&viewInterface;stats[11]++;return &extension;}
    SB::tresult PLUGIN_API setActive(SB::TBool value)override{active=value!=0;return 0;}
    SB::tresult PLUGIN_API setupProcessing(V::ProcessSetup&s)override{sampleRate=s.sampleRate;return BusTestPlugin::setupProcessing(s);}
    SB::tresult PLUGIN_API process(V::ProcessData&p)override{stats[18]++;for(int c=0;c<2;c++)for(int n=0;n<p.numSamples;n++){float value=p.inputs[0].channelBuffers32[c][n];if(d&&p.processContext&&(p.processContext->state&V::ProcessContext::kPlaying)){double time=(p.processContext->projectTimeSamples+n)/sampleRate;for(auto*region:d->playback){auto&rp=region->props;auto&s=*region->mod->source;if(!s.enabled||time<rp.startInPlaybackTime||time>=rp.startInPlaybackTime+rp.durationInPlaybackTime)continue;auto index=(size_t)std::llround((rp.startInModificationTime+time-rp.startInPlaybackTime)*s.props.sampleRate);auto&data=c?s.right:s.left;if(index<data.size())value+=(float)(data[index]*region->mod->gain);}}p.outputs[0].channelBuffers32[c][n]=value;}return 0;}
};
class VstFactory:public SB::IPluginFactory{
public:
    SB::tresult PLUGIN_API queryInterface(const SB::TUID iid,void**p)override{*p=nullptr;if(memcmp(iid,SB::IPluginFactory_iid,16)&&memcmp(iid,SB::FUnknown_iid,16))return SB::kNoInterface;*p=this;return 0;}SB::uint32 PLUGIN_API addRef()override{return 1;}SB::uint32 PLUGIN_API release()override{return 1;}
    SB::tresult PLUGIN_API getFactoryInfo(SB::PFactoryInfo*p)override{*p={};strcpy(p->vendor,"Karsei Test");return 0;}SB::int32 PLUGIN_API countClasses()override{return 2;}
    SB::tresult PLUGIN_API getClassInfo(SB::int32 n,SB::PClassInfo*p)override{if(n<0||n>1)return SB::kInvalidArgument;*p={};memcpy(p->cid,n?"VampAraFactory01":"VampAraAudio0001",16);strcpy(p->name,"ARA Contract Fixture");strcpy(p->category,n?kARAMainFactoryClass:kVstAudioEffectClass);return 0;}
    SB::tresult PLUGIN_API createInstance(SB::FIDString,SB::FIDString iid,void**p)override{auto*plug=new Fixture;auto result=plug->queryInterface(iid,p);plug->release();return result;}
}vstFactory;
}
extern "C" __declspec(dllexport) SB::IPluginFactory*PLUGIN_API GetPluginFactory(){return &vstFactory;}
extern "C" __declspec(dllexport) bool PLUGIN_API InitDll(){return true;}
extern "C" __declspec(dllexport) bool PLUGIN_API ExitDll(){return true;}
extern "C" __declspec(dllexport) const uint64_t*FixtureAraStats(){return stats.data();}
extern "C" __declspec(dllexport) void FixtureAraEdit(double gain){for(auto*d:documents){for(auto*m:d->mods)m->gain=gain;d->dirty=true;}}
extern "C" __declspec(dllexport) void FixtureAraTransport(){for(auto*d:documents)d->transport=true;}
