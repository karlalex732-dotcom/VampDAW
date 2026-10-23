#define INIT_CLASS_IID
#include <windows.h>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include <iostream>
namespace SB=Steinberg;namespace V=Steinberg::Vst;
int main(){CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);wchar_t binary[32768]{};GetFullPathNameW(L"build/Oracle Rack.vst3/Contents/x86_64-win/Oracle Rack.vst3",32768,binary,nullptr);auto module=LoadLibraryExW(binary,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!module){std::cerr<<"Load failed "<<GetLastError()<<std::endl;return 1;}auto init=(bool(PLUGIN_API*)())GetProcAddress(module,"InitDll");auto get=(SB::IPluginFactory*(PLUGIN_API*)())GetProcAddress(module,"GetPluginFactory");if(!init||!init()||!get)return 2;auto*factory=get();SB::PClassInfo info{};if(factory->getClassInfo(0,&info)!=0)return 3;V::IComponent*component=nullptr;if(factory->createInstance(info.cid,V::IComponent_iid,(void**)&component)!=0)return 4;if(component->initialize(nullptr)!=0)return 5;component->terminate();component->release();factory->release();auto exit=(bool(PLUGIN_API*)())GetProcAddress(module,"ExitDll");if(!exit||!exit())return 6;FreeLibrary(module);CoUninitialize();std::cout<<"Oracle loads in a host without a Common Controls manifest"<<std::endl;return 0;}
