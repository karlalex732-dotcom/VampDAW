#include "plugin.hpp"
#include <shellapi.h>
using namespace vamp;
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){int count=0;auto**args=CommandLineToArgvW(GetCommandLineW(),&count);if(count!=3){LocalFree(args);return 2;}CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);int result=1;try{auto j=describePlugin(fs::path(args[1]));std::ofstream out{fs::path(args[2])};out<<j.dump(2);result=out?0:1;}catch(...){result=1;}CoUninitialize();LocalFree(args);return result;}
