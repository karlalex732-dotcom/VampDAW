#pragma once
#include "model.hpp"
#include <shlobj.h>
#include <stdexcept>

namespace vamp {
inline fs::path knownDirectory(REFKNOWNFOLDERID id) {
    PWSTR value=nullptr;
    HRESULT status=SHGetKnownFolderPath(id,KF_FLAG_DEFAULT,nullptr,&value);
    if(FAILED(status))throw std::runtime_error("Windows user folder is unavailable");
    fs::path path(value);CoTaskMemFree(value);return path;
}
inline fs::path executableDirectory() {
    std::wstring value(32768,L'\0');
    DWORD length=GetModuleFileNameW(nullptr,value.data(),(DWORD)value.size());
    if(!length||length>=value.size())throw std::runtime_error("Cannot locate VampDAW executable");
    value.resize(length);return fs::path(value).parent_path();
}
struct AppPaths {
    fs::path binaries,data,state;
    bool installed=false;
    static AppPaths resolve(const fs::path& executable,const fs::path& documents,const fs::path& local,bool installedMode) {
        AppPaths p;p.binaries=executable;p.installed=installedMode;
        if(!installedMode&&p.binaries.filename()==L"build")p.binaries=p.binaries.parent_path();
        p.data=installedMode?documents/L"VampDAW":p.binaries;
        p.state=installedMode?local/L"VampDAW":p.data/L"projects";
        return p;
    }
    static AppPaths current() {
        auto binary=executableDirectory();bool installed=fs::exists(binary/L"VampDAW.installed");
        return resolve(binary,installed?knownDirectory(FOLDERID_Documents):fs::path{},installed?knownDirectory(FOLDERID_LocalAppData):fs::path{},installed);
    }
    void prepare() const {
        for(auto& p:{data/L"projects/assets",data/L"renders",data/L"User Sounds",data/L"User Plugins",state,state/L"work/catalog",state/L"logs"})fs::create_directories(p);
    }
    std::vector<fs::path> pluginDirectories() const {
        return {knownDirectory(FOLDERID_ProgramFilesCommonX64)/L"VST3",data/L"User Plugins",binaries/L"plugins",knownDirectory(FOLDERID_ProgramFilesX64)/L"VstPlugins",knownDirectory(FOLDERID_ProgramFilesX64)/L"Steinberg/VstPlugins",knownDirectory(FOLDERID_ProgramFilesCommonX64)/L"VST2",knownDirectory(FOLDERID_ProgramFilesCommonX64)/L"Steinberg/VST2",knownDirectory(FOLDERID_LocalAppData)/L"Programs/Common/VST3"};
    }
};
}
