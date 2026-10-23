#include "../third_party/vestige/aeffectx.h"
#include <windows.h>
#include <cstring>
#include <cmath>
#include <array>
#ifndef FIXTURE_KIND
#define FIXTURE_KIND 0
#endif
namespace {
struct Fixture{AEffect effect{};std::array<char,96>tail{};audioMasterCallback host;float gain=.5f,cc=0;double rate=0;bool active=false,started=false;int note=-1,program=0;struct Event{int at,status,key,value;};std::array<Event,16384>events{};int count=0;HWND child=nullptr;};
Fixture&self(AEffect*e){return*(Fixture*)e->ptr3;}
void VST_CALL_CONV set(AEffect*e,int id,float v){if(id==0)self(e).gain=v;}
float VST_CALL_CONV get(AEffect*e,int id){return id==0?self(e).gain:0;}
void VST_CALL_CONV process(AEffect*e,float**in,float**out,int frames){auto&f=self(e);auto*time=(VstTimeInfo*)f.host(e,audioMasterGetTime,0,0,nullptr,0);bool valid=f.active&&f.started&&time&&time->sampleRate==f.rate&&std::isfinite(time->ppqPos)&&time->tempo>0;for(int i=0;i<frames;i++){for(int n=0;n<f.count;n++)if(f.events[n].at==i){auto event=f.events[n];if(event.status==0x90&&event.value)f.note=event.key;else if(event.status==0x80||event.status==0x90)f.note=-1;else if(event.status==0xb0&&event.key==1)f.cc=event.value/127.f;}
    for(int c=0;c<e->numOutputs;c++){float signal=FIXTURE_KIND==1?(f.note>=0?.2f:0)+f.cc*.1f:e->numInputs==1?in[0][i]:in[c%2][i];if(FIXTURE_KIND==3)signal+=in[2+c%2][i]*.25f;out[c][i]=valid?signal*f.gain:NAN;}}f.count=0;}
intptr_t VST_CALL_CONV dispatch(AEffect*e,int op,int index,intptr_t value,void*ptr,float opt){auto&f=self(e);switch(op){case effOpen:return 1;case effClose:if(f.child)DestroyWindow(f.child);delete&f;return 1;case effSetSampleRate:f.rate=opt;return 1;case effSetBlockSize:return value>0;case effMainsChanged:f.active=value;return 1;case 71:f.started=true;return 1;case 72:f.started=false;return 1;case 35:return FIXTURE_KIND==1?2:1;
case effGetEffectName:if(ptr)strcpy((char*)ptr,FIXTURE_KIND==1?"Vamp Test Synth":"Vamp Test FX");return 1;case effGetVendorString:if(ptr)strcpy((char*)ptr,"VampDAW test fixture");return 1;case effGetParamName:if(ptr)strcpy((char*)ptr,"Gain");return 1;case effGetProgram:return f.program;case effSetProgram:f.program=(int)value;return 1;
case effGetChunk:if(ptr)*(void**)ptr=&f.gain;return sizeof f.gain;case effSetChunk:if(value==sizeof(float)&&ptr)memcpy(&f.gain,ptr,sizeof f.gain);return 1;
case effCanDo:return ptr&&!strcmp((char*)ptr,"receiveVstMidiEvent")&&FIXTURE_KIND==1?1:0;
case effProcessEvents:{auto*packet=(VstEvents*)ptr;f.count=0;for(int i=0;i<packet->numEvents&&f.count<(int)f.events.size();i++){auto*event=(VstMidiEvent*)packet->events[i];if(event->type==kVstMidiType)f.events[f.count++]={event->deltaFrames,event->midiData[0]&0xf0,event->midiData[1]&127,event->midiData[2]&127};}return 1;}
case effEditOpen:f.child=CreateWindowExW(0,L"STATIC",L"VST2 fixture",WS_CHILD|WS_VISIBLE,0,0,320,180,(HWND)ptr,nullptr,GetModuleHandleW(nullptr),nullptr);return 1;
case effEditGetRect:{static int16_t rect[]={0,0,180,320};*(void**)ptr=rect;return 1;}case effEditClose:if(f.child)DestroyWindow(f.child);f.child=nullptr;return 1;default:return 0;}}
}
extern "C" __declspec(dllexport) AEffect* VST_CALL_CONV VSTPluginMain(audioMasterCallback host){if(host(nullptr,audioMasterVersion,0,0,nullptr,0)<2400)return nullptr;auto*f=new Fixture;f->host=host;auto&e=f->effect;e.magic=kEffectMagic;e.dispatcher=dispatch;e.setParameter=set;e.getParameter=get;e.processReplacing=process;e.numParams=1;e.numPrograms=2;e.numInputs=FIXTURE_KIND==1?0:FIXTURE_KIND==2?1:FIXTURE_KIND==3?4:2;e.numOutputs=FIXTURE_KIND==2?1:2;e.flags=effFlagsCanReplacing|effFlagsHasEditor|(FIXTURE_KIND!=2?(1<<5):0)|(FIXTURE_KIND==1?effFlagsIsSynth:0);e.ptr3=f;e.uniqueID=0x56413200+FIXTURE_KIND;int delay=64;memcpy(e.empty3,&delay,4);return&e;}
