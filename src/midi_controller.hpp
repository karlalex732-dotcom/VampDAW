#pragma once
#include "model.hpp"
#include <cwctype>
namespace vamp {
enum class ControllerProfile{Auto,Generic,Mackie,Arturia,Launchkey3,Akai,Native};
enum class ControlAction{None,Play,Stop,Record,Loop,Click,Undo,Redo,Save,PreviousTrack,NextTrack,PreviousBank,NextBank,Seek,Volume,Pan,Mute,Solo,Arm,Plugin};
struct ControllerEvent{ControlAction action=ControlAction::None;int index=-1;double value=0;bool relative=false;};
struct MidiBinding{int status=0xb0,channel=0,number=0,mode=0;ControlAction action=ControlAction::Volume;int index=-1;};
inline const wchar_t*controllerProfileName(ControllerProfile p){static const wchar_t*names[]={L"Auto detect",L"Generic MIDI / Learn",L"Mackie Control Universal",L"Arturia KeyLab Essential mk3 / MCU",L"Novation Launchkey MK3 / DAW",L"Akai / MIDI Learn",L"Native Instruments / MIDI Learn"};return names[(int)clamp((int)p,0,6)];}
inline ControllerProfile detectController(std::wstring name){for(auto&c:name)c=std::towlower(c);if(name.find(L"launchkey")!=name.npos&&name.find(L"mk3")!=name.npos&&name.find(L"mini")==name.npos)return ControllerProfile::Launchkey3;if((name.find(L"keylab")!=name.npos||name.find(L"kl essential")!=name.npos)&&name.find(L"essential")!=name.npos)return ControllerProfile::Arturia;if(name.find(L"mackie")!=name.npos||name.find(L"x-touch")!=name.npos)return ControllerProfile::Mackie;if(name.find(L"akai")!=name.npos||name.find(L"mpk")!=name.npos||name.find(L"mpd")!=name.npos)return ControllerProfile::Akai;if(name.find(L"komplete")!=name.npos||name.find(L"kontrol")!=name.npos||name.find(L"maschine")!=name.npos)return ControllerProfile::Native;return ControllerProfile::Generic;}
inline double relativeMidi(int value,int mode){value&=127;switch(mode){case 1:return(value&64)?-(value&63):(value&63);case 2:return value<64?value:value-128;case 3:return value-64;case 4:return value-16;default:return 0;}}
inline bool validBinding(const MidiBinding&b){return(b.status==0xb0||b.status==0x90||b.status==0xe0)&&b.channel>=0&&b.channel<16&&b.number>=0&&b.number<128&&b.mode>=0&&b.mode<=4&&(int)b.action>0&&(int)b.action<=(int)ControlAction::Plugin&&b.index>=-1&&b.index<=8;}
inline Json bindingsJson(const std::vector<MidiBinding>&bindings){Json out=Json::array();for(auto&b:bindings)if(validBinding(b))out.push_back({{"status",b.status},{"channel",b.channel},{"number",b.number},{"mode",b.mode},{"action",(int)b.action},{"index",b.index}});return out;}
inline std::vector<MidiBinding>readBindings(const Json&j){std::vector<MidiBinding>out;if(!j.is_array())return out;for(auto&v:j){if(out.size()>=256)break;try{if(!v.is_object())continue;MidiBinding b{v.at("status").get<int>(),v.at("channel").get<int>(),v.at("number").get<int>(),v.value("mode",0),(ControlAction)v.at("action").get<int>(),v.value("index",-1)};if(validBinding(b))out.push_back(b);}catch(...){}}return out;}
inline bool sameMidiControl(const MidiBinding&a,const MidiBinding&b){return a.status==b.status&&a.channel==b.channel&&(a.status==0xe0||a.number==b.number);}
inline bool controlButton(ControlAction a){return a!=ControlAction::Volume&&a!=ControlAction::Pan&&a!=ControlAction::Seek&&a!=ControlAction::Plugin;}
class ControllerDecoder{
    std::array<bool,4096>pressed{};
public:
    int potMode=3,faderMode=1,padMode=2;bool armMode=false,padsSolo=false;std::array<bool,9>touched{};
    void reset(){pressed.fill(false);touched.fill(false);potMode=3;faderMode=1;padMode=2;armMode=false;padsSolo=false;}
    bool edge(int key,bool down){bool old=pressed[key];pressed[key]=down;return down&&!old;}
    ControllerEvent decode(ControllerProfile profile,uint32_t message,const std::vector<MidiBinding>&bindings){
        int status=message&0xf0,ch=message&15,n=(message>>8)&127,v=(message>>16)&127;bool down=status!=0x80&&v>0;int kind=(status==0x80?0x90:status);bool rise=false;if(kind==0x90||kind==0xb0)rise=edge((kind==0x90?2048:0)+ch*128+n,down);
        for(auto&b:bindings)if(b.status==kind&&b.channel==ch&&(kind==0xe0||b.number==n)){if(controlButton(b.action)){if(rise)return{b.action,b.index,1,false};return{};}double value=kind==0xe0?(n+(v<<7))/16383.:v/127.;return{b.action,b.index,b.mode?relativeMidi(v,b.mode):value,b.mode!=0};}
        if(profile==ControllerProfile::Mackie||profile==ControllerProfile::Arturia){
            if(kind==0x90&&ch==0&&n>=104&&n<=112){touched[n-104]=down;return{};}
            if(status==0xe0&&ch<=8)return{ControlAction::Volume,ch,(n+(v<<7))/16383.,false};
            if(status==0xb0&&ch==0&&n>=16&&n<24)return{ControlAction::Pan,n-16,relativeMidi(v,1),true};
            if(status==0xb0&&ch==0&&n==60)return{ControlAction::Seek,-1,relativeMidi(v,1),true};
            if(kind==0x90&&ch==0&&rise){if(n<8)return{ControlAction::Arm,n,1};if(n<16)return{ControlAction::Solo,n-8,1};if(n<24)return{ControlAction::Mute,n-16,1};if(n<32)return{ControlAction::NextTrack,n-24,1};switch(n){case 46:return{ControlAction::PreviousBank};case 47:return{ControlAction::NextBank};case 48:return{ControlAction::PreviousTrack};case 49:return{ControlAction::NextTrack};case 80:return{ControlAction::Save};case 81:return{ControlAction::Undo};case 86:return{ControlAction::Loop};case 89:return{ControlAction::Click};case 91:return{ControlAction::Seek,-1,-4,true};case 92:return{ControlAction::Seek,-1,4,true};case 93:return{ControlAction::Stop};case 94:return{ControlAction::Play};case 95:return{ControlAction::Record};case 96:return{ControlAction::PreviousTrack};case 97:return{ControlAction::NextTrack};case 98:return{ControlAction::PreviousBank};case 99:return{ControlAction::NextBank};}}
        }
        if(profile==ControllerProfile::Launchkey3&&status==0xb0&&ch==15){
            if(n==9){potMode=v;return{};}if(n==10){faderMode=v;return{};}if(n==3){padMode=v;return{};}
            if(n>=21&&n<=28){if(potMode==1)return{ControlAction::Volume,n-21,v/127.};if(potMode==3)return{ControlAction::Pan,n-21,v/127.};if(potMode==2)return{ControlAction::Plugin,n-21,v/127.};}
            if(n>=53&&n<=61&&faderMode==1)return{ControlAction::Volume,n-53,v/127.};
            if(rise){if(n>=37&&n<=44)return{armMode?ControlAction::Arm:ControlAction::NextTrack,n-37,1};switch(n){case 45:armMode=!armMode;return{};case 76:return{ControlAction::Click};case 77:return{ControlAction::Undo};case 102:return{ControlAction::PreviousTrack};case 103:return{ControlAction::NextTrack};case 104:return{ControlAction::PreviousBank};case 105:return{ControlAction::NextBank};case 107:padsSolo=!padsSolo;return{};case 115:return{ControlAction::Play};case 116:return{ControlAction::Stop};case 117:return{ControlAction::Record};case 118:return{ControlAction::Loop};}}
        }
        if(profile==ControllerProfile::Launchkey3&&kind==0x90&&ch==0&&rise&&padMode==2){if(n>=96&&n<=103)return{ControlAction::NextTrack,n-96,1};if(n>=112&&n<=119)return{padsSolo?ControlAction::Solo:ControlAction::Mute,n-112,1};}
        if(message==0xfa||message==0xfb)return{ControlAction::Play};if(message==0xfc)return{ControlAction::Stop};return{};
    }
};
struct SoftPickup{bool latched=false;double previous=-1,target=-1;bool accept(double physical,double current){if(target>=0&&std::abs(current-target)>.015)latched=false;target=current;if(latched){previous=physical;target=physical;return true;}bool crossed=previous>=0&&(physical-current)*(previous-current)<=0;previous=physical;if(std::abs(physical-current)<.025||crossed){latched=true;target=physical;return true;}return false;}void reset(){latched=false;previous=target=-1;}};
}
