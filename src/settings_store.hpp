#pragma once
#include "model.hpp"
#include <cwctype>
#include <stdexcept>

namespace vamp {
inline std::wstring folderIdentity(const std::string&path){auto value=fs::path(wide(path)).lexically_normal().wstring();while(value.size()>3&&(value.back()==L'/'||value.back()==L'\\'))value.pop_back();for(auto&c:value)c=c==L'/'?L'\\':std::towlower(c);return value;}
inline Json cleanFolderList(const Json&value){Json out=Json::array();std::vector<std::wstring>seen;if(value.is_array())for(auto&item:value)if(item.is_string()){auto text=item.get<std::string>();if(text.empty())continue;auto key=folderIdentity(text);if(std::find(seen.begin(),seen.end(),key)==seen.end()){seen.push_back(key);out.push_back(text);}}return out;}
inline Json mergeFolderLists(const Json&base,const Json&local,const Json&remote){
    auto b=cleanFolderList(base),l=cleanFolderList(local),r=cleanFolderList(remote);auto has=[](const Json&list,const Json&value){auto key=folderIdentity(value.get<std::string>());return std::any_of(list.begin(),list.end(),[&](auto&item){return folderIdentity(item.template get<std::string>())==key;});};
    Json out=Json::array();for(auto&item:l)if(!has(b,item)||has(r,item))out.push_back(item);for(auto&item:r)if(!has(b,item)&&!has(out,item))out.push_back(item);return out;
}
class SettingsStore{
    Json baseline=Json::object();
    struct Lock{HANDLE h=nullptr;explicit Lock(const fs::path&path){uint64_t hash=1469598103934665603ull;auto name=path.lexically_normal().wstring();for(auto c:name){hash^=std::towlower(c);hash*=1099511628211ull;}auto id=L"Local\\VampDAW.Settings."+std::to_wstring(hash);h=CreateMutexW(nullptr,FALSE,id.c_str());if(!h)throw std::runtime_error("Cannot lock settings");auto status=WaitForSingleObject(h,2500);if(status!=WAIT_OBJECT_0&&status!=WAIT_ABANDONED){CloseHandle(h);h=nullptr;throw std::runtime_error("Settings are busy. Try again.");}}~Lock(){if(h){ReleaseMutex(h);CloseHandle(h);}}};
    static bool readFile(const fs::path&p,Json&out){try{std::ifstream f(p,std::ios::binary);if(!f)return false;f>>out;return out.is_object();}catch(...){return false;}}
public:
    bool recovered=false;
    Json load(const fs::path&path){Lock lock(path);Json value;if(readFile(path,value)){recovered=false;return value;}auto backup=path;backup+=L".bak";recovered=readFile(backup,value);return recovered?value:Json::object();}
    void initialize(const Json&effective){baseline=effective;}
    void save(const fs::path&path,const Json&local){
        Lock lock(path);Json remote;bool valid=readFile(path,remote);if(!valid){auto backup=path;backup+=L".bak";if(!readFile(backup,remote))remote=Json::object();}
        Json merged=remote;for(auto it=local.begin();it!=local.end();++it){bool changed=!baseline.contains(it.key())||baseline[it.key()]!=it.value();if(!changed&&remote.contains(it.key()))continue;if((it.key()=="soundRoots"||it.key()=="pluginRoots")&&changed&&baseline.contains(it.key())&&remote.contains(it.key()))merged[it.key()]=mergeFolderLists(baseline[it.key()],it.value(),remote[it.key()]);else merged[it.key()]=it.value();}
        fs::create_directories(path.parent_path());auto temporary=path;temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId());auto text=merged.dump(2);
        {std::ofstream f(temporary,std::ios::binary|std::ios::trunc);if(!f||!f.write(text.data(),text.size()))throw std::runtime_error("Cannot save settings");f.flush();if(!f)throw std::runtime_error("Cannot save settings");}
        auto backup=path;backup+=L".bak";if(valid&&!CopyFileW(path.c_str(),backup.c_str(),FALSE))throw std::runtime_error("Cannot back up settings");
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace settings file");baseline=local;
    }
};
}
