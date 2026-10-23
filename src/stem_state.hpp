#pragma once
#include "model.hpp"
#include <cstdio>
#include <stdexcept>

namespace vamp {
// UI state can be absent before the first job, or come from older settings.
inline bool stemFlag(const Json& object,const char* key,bool fallback=false) {
    if(!object.is_object())return fallback;
    auto it=object.find(key);return it!=object.end()&&it->is_boolean()?it->get<bool>():fallback;
}
inline std::string stemText(const Json& object,const char* key,const std::string& fallback={}) {
    if(!object.is_object())return fallback;
    auto it=object.find(key);return it!=object.end()&&it->is_string()?it->get<std::string>():fallback;
}
inline int stemIndex(const Json& object,const std::string& key,int fallback,int count) {
    if(!object.is_object()||count<=0)return fallback;
    auto it=object.find(key);if(it==object.end()||!it->is_number_integer())return fallback;
    if(it->is_number_unsigned()){auto value=it->get<uint64_t>();return value>=(uint64_t)count?count-1:(int)value;}
    auto value=it->get<int64_t>();return (int)std::max<int64_t>(0,std::min<int64_t>(value,count-1));
}
inline std::wstring stemStatusText(bool running,const Json& result,const std::wstring& stage) {
    if(running)return stage.empty()?L"Preparing audio":stage;
    if(stemFlag(result,"passed"))return L"Complete — preview stems or add them to Playlist";
    return stage.empty()?L"Ready. Choose audio, quality and the stems you need.":stage;
}
inline void validateStemResult(const Json& result) {
    if(!result.is_object()||!result.contains("passed")||!result.at("passed").is_boolean())
        throw std::runtime_error("Invalid response from stem engine");
    if(!stemFlag(result,"passed"))return;
    auto it=result.find("sources");
    if(it==result.end()||!it->is_array()||it->empty()||it->size()>16)
        throw std::runtime_error("Invalid stem source list");
    for(const auto& source:*it) {
        if(stemText(source,"path").empty()||stemText(source,"name").empty()||
           (source.contains("import_path")&&(!source.at("import_path").is_string()||source.at("import_path").get_ref<const std::string&>().empty())))
            throw std::runtime_error("Invalid stem source description");
    }
}
inline std::string stemExitMessage(DWORD code) {
    char hex[16]{};std::snprintf(hex,sizeof hex,"0x%08lX",(unsigned long)code);
    std::string message=std::string("Stem engine exited (")+hex+")";
    if(code==0xC0000135UL||code==0xC0000139UL)message+=". A required runtime DLL is missing or incompatible; reinstall with the Blood Stems component";
    else if(code==0xC000001DUL)message+=". The stem runtime used a CPU instruction this processor does not support";
    else if(code==0xC0000005UL||code==0xC0000409UL)message+=". The isolated stem engine crashed";
    else if(code==0xC0000017UL)message+=". Not enough memory; choose a shorter segment and fewer shift passes";
    return message;
}
inline void appendStemLog(const fs::path& path,const char* event,const std::string& detail={}) noexcept {
    try {
        fs::create_directories(path.parent_path());
        // Keep a bounded previous log for repeat failures on another PC.
        std::error_code ec;if(fs::file_size(path,ec)>2*1024*1024&&!ec) {
            auto previous=path;previous+=L".previous";fs::remove(previous,ec);ec.clear();fs::rename(path,previous,ec);
        }
        SYSTEMTIME time{};GetSystemTime(&time);char stamp[40]{};
        std::snprintf(stamp,sizeof stamp,"%04u-%02u-%02uT%02u:%02u:%02uZ",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond);
        std::ofstream file(path,std::ios::app);file<<Json({{"time",stamp},{"pid",GetCurrentProcessId()},{"event",event},{"detail",detail}}).dump(-1,' ',false,Json::error_handler_t::replace)<<'\n';
    }catch(...){} // Diagnostics must never take down the DAW.
}
}
