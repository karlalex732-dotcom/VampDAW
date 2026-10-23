#include "../third_party/vestige/aeffectx.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace {
struct Fixture{AEffect effect{};std::array<float,6>values{.5f,.5f,.5f,.5f,.5f,0};};
Fixture&self(AEffect*e){return*(Fixture*)e->ptr3;}
double physical(int index,double n){switch(index){case 0:return-24+n*48;case 1:return-60+n*60;case 2:return std::pow(20.,n);case 3:return .1*std::pow(2000.,n);case 4:return 10*std::pow(200.,n);default:return 0;}}
void VST_CALL_CONV set(AEffect*e,int id,float n){if(id>=0&&id<6)self(e).values[id]=n;}
float VST_CALL_CONV get(AEffect*e,int id){return id>=0&&id<6?self(e).values[id]:0;}
void VST_CALL_CONV process(AEffect*e,float**in,float**out,int frames){float gain=(float)std::pow(10.,physical(0,self(e).values[0])/20);for(int c=0;c<2;c++)for(int n=0;n<frames;n++)out[c][n]=in[c][n]*gain;}
intptr_t VST_CALL_CONV dispatch(AEffect*e,int op,int index,intptr_t value,void*ptr,float){auto&f=self(e);switch(op){case effOpen:return 1;case effClose:delete&f;return 1;case effGetEffectName:if(ptr)strcpy((char*)ptr,"Oracle Mapping Compressor");return 1;case effGetVendorString:if(ptr)strcpy((char*)ptr,"Karsei Test");return 1;case effGetParamName:{const char*names[]={"Gain","Threshold","Ratio","Attack","Release","Character"};if(ptr&&index>=0&&index<6)strcpy((char*)ptr,names[index]);return 1;}case effGetParamLabel:if(ptr)strcpy((char*)ptr,index<2?"dB":index==3||index==4?"ms":"");return 1;case effGetParamDisplay:if(ptr&&index>=0&&index<6){if(index==5)strcpy((char*)ptr,"Crimson");else std::snprintf((char*)ptr,32,"%.4f",physical(index,f.values[index]));}return 1;case effGetChunk:if(ptr)*(void**)ptr=f.values.data();return sizeof(f.values);case effSetChunk:if(ptr&&value==sizeof(f.values))memcpy(f.values.data(),ptr,sizeof(f.values));return 1;case 35:return 1;default:return 0;}}
}
extern "C" __declspec(dllexport) AEffect* VST_CALL_CONV VSTPluginMain(audioMasterCallback host){if(host(nullptr,audioMasterVersion,0,0,nullptr,0)<2400)return nullptr;auto*f=new Fixture;auto&e=f->effect;e.magic=kEffectMagic;e.dispatcher=dispatch;e.setParameter=set;e.getParameter=get;e.processReplacing=process;e.numParams=6;e.numInputs=e.numOutputs=2;e.numPrograms=1;e.flags=effFlagsCanReplacing|(1<<5);e.ptr3=f;e.uniqueID=0x4F52434D;return&e;}
