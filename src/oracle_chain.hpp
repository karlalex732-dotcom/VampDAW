#pragma once
#include "assistant.hpp"
#include "plugin.hpp"
#include <cctype>
#include <cstring>
namespace vamp {
inline std::string oracleKey(std::string s){std::string result;for(unsigned char c:s)if(std::isalnum(c))result+=(char)std::tolower(c);return result;}
inline std::string oracleControlKey(std::string name,std::string unit){auto key=oracleKey(name),suffix=oracleKey(unit);if(!suffix.empty()&&key.size()>suffix.size()&&key.compare(key.size()-suffix.size(),suffix.size(),suffix)==0)key.resize(key.size()-suffix.size());for(auto prefix:{"compressor","compression","dynamics","equalizer","filter","comp","eq"}){size_t n=std::strlen(prefix);if(key.size()>n&&key.compare(0,n,prefix)==0&&key.rfind("compensation",0)!=0){key.erase(0,n);break;}}return key;}
inline bool oracleExternalEffect(const Json&j){if(!j.is_object()||!j.value("ok",false)||j.value("instrument",false))return false;auto path=j.value("path","");auto name=oracleKey(j.value("name",""));return !path.empty()&&path.find("@vamp")==std::string::npos&&name.find("oraclerack")==std::string::npos;}
inline int oracleRoleScore(int kind,const Json&entry){
    auto name=oracleKey(entry.value("name","")),category=oracleKey(entry.value("category",""));auto has=[&](const char*s){return name.find(s)!=std::string::npos;};auto cat=[&](const char*s){return category.find(s)!=std::string::npos;};
    switch(kind){
    case 0:return has("proq")||has("nova")||has("equalizer")||has("parametric")?90:cat("eq")?65:0;
    case 2:return has("filter")||has("locut")||has("lowcut")?75:0;
    case 3:return has("compressor")||has("proc2")||has("proc3")||has("comp")?80:0;
    case 8:return has("expander")||has("gate")?80:0;
    case 9:return has("deesser")||has("prods")||has("sibilance")?95:0;
    case 12:return has("saturat")||has("decapitator")||has("saturn")||has("distort")?80:cat("distortion")?65:0;
    case 18:return has("reverb")||has("room")||has("vintageverb")||has("prore")?80:cat("reverb")?65:0;
    case 26:return has("imager")||has("stereowidth")||has("stereoenhancer")?85:0;
    case 27:return has("gain")||has("trim")?70:0;
    case 31:return has("limiter")||has("prol")||has("maximizer")?90:0;
    case 32:return has("dcblock")||has("dcremove")?90:0;
    case 37:return has("deplosive")||has("plosive")?90:0;
    case 41:return has("delay")||has("echo")?80:cat("delay")?65:0;
    }return 0;
}
inline AssistantPlan resolveOraclePlan(const AssistantPlan&original,bool any,const Json&catalog,const Json&overrides){
    auto plan=original;for(auto&step:plan.steps){step.path.clear();step.name.clear();if(!any)continue;auto key=std::to_string(step.kind);const Json*chosen=nullptr;
        if(overrides.is_object()&&overrides.contains(key)&&overrides[key].is_string()){auto path=overrides[key].get<std::string>();if(path.empty())continue;for(auto&entry:catalog)if(oracleExternalEffect(entry)&&entry.value("path","")==path){chosen=&entry;break;}}
        else{int score=0;for(auto&entry:catalog)if(oracleExternalEffect(entry)){int candidate=oracleRoleScore(step.kind,entry);if(candidate>score){score=candidate;chosen=&entry;}}}
        if(chosen){step.path=chosen->value("path","");step.name=chosen->value("name","");}
    }return plan;
}
// External parameter mapping uses the plugin's own text conversion. No guessed normalized units.
int configureOracleEffect(Plugin&,const AssistantStep&,Json*controls=nullptr);
std::shared_ptr<Plugin>loadOracleEffect(const AssistantStep&,double rate,int*mapped=nullptr,Json*controls=nullptr);
struct OraclePreparedStage{AssistantStep step;std::shared_ptr<Plugin>effect;int mapped=0;double inputRms=-120,outputRms=-120,peak=-120;};
struct OraclePreparedChain{std::vector<OraclePreparedStage>stages;bool calibrated=false;Json report=Json::array();};
OraclePreparedChain buildOracleChain(const AssistantPlan&,double rate,const AssistantCapture*capture=nullptr,const std::function<void(size_t,size_t)>&progress={},double tempo=120,double beat=0);
inline Json oracleStepJson(const AssistantStep&s){return{{"kind",s.kind},{"reason",s.reason},{"settings",s.settings},{"path",s.path},{"name",s.name},{"bypass",s.bypass}};}
inline AssistantStep oracleStepFromJson(const Json&j){AssistantStep s;s.kind=j.value("kind",0);if(s.kind<0||s.kind>=46)throw std::runtime_error("Invalid Oracle effect role");s.reason=j.value("reason","");s.settings=j.value("settings",std::vector<std::pair<int,double>>{});if(s.settings.size()>32)throw std::runtime_error("Too many Oracle parameters");for(auto&p:s.settings)if(p.first<0||p.first>=32||!std::isfinite(p.second))throw std::runtime_error("Invalid Oracle parameter");s.path=j.value("path","");s.name=j.value("name","");s.bypass=j.value("bypass",false);return s;}
}
