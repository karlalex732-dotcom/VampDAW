#include "oracle_rack.hpp"
#include "plugin_discovery.hpp"
#include "localization.hpp"
#include "ui_prompt.hpp"
#include "oracle_picker.hpp"
#include <commdlg.h>
#include <shobjidl.h>
#include <sstream>
namespace vamp {
namespace {
fs::path oracleDataDirectory(){wchar_t path[32768]{};auto length=GetEnvironmentVariableW(L"ORACLE_DATA_DIR",path,32768);return length&&length<32768?fs::path(path):knownDirectory(FOLDERID_LocalAppData)/L"Karsei/Oracle Rack";}
fs::path oracleModuleDirectory(){HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)&oracleModuleDirectory,&module);wchar_t path[32768]{};GetModuleFileNameW(module,path,32768);return fs::path(path).parent_path();}
Json readOracleJson(const fs::path&path,size_t limit=16*1024*1024){std::error_code ec;if(fs::file_size(path,ec)>limit||ec)return{};try{std::ifstream file(path);Json j;file>>j;return j;}catch(...){return{};}}
std::wstring quote(const std::wstring&s){std::wstring result=L"\"";size_t slashes=0;for(auto c:s){if(c==L'\\'){slashes++;continue;}if(c==L'\"')result.append(slashes*2+1,L'\\');else result.append(slashes,L'\\');slashes=0;result+=c;}result.append(slashes*2,L'\\');return result+L'\"';}
}
OracleRack::OracleRack(){
    data=oracleDataDirectory();moduleDirectory=oracleModuleDirectory();oracle.assistantStandalone=oracle.assistantAnyPlugins=true;oracle.assistantMaster=true;oracle.set(0,oracle.spec().parameters[0].normalized(25));readLibrary();publish(std::make_unique<Chain>());
    oracle.assistantResolve=[this](const AssistantPlan&p){return resolveOraclePlan(p,true,catalog,oracle.assistantOverrides);};
    oracle.assistantChoose=[this](int row){if(row<0||row>=(int)oracle.assistantPlan.steps.size())return false;auto&step=oracle.assistantPlan.steps[row];auto chosen=chooseOracleEffect(oracle.window,catalog,step);if(!chosen)return false;auto key=std::to_string(step.kind);if(*chosen=="@automatic")oracle.assistantOverrides.erase(key);else oracle.assistantOverrides[key]=*chosen;return true;};
    oracle.assistantApply=[this](const AssistantPlan&p){return apply(p);};oracle.assistantLibrary=[this]{libraryMenu();};oracle.assistantPoll=[this]{poll();};oracle.assistantOpen=[this](int index){auto*c=chain();if(!oracle.assistantApplied||!c||index<0||index>=(int)c->stages.size()||!c->stages[index].effect)return false;c->stages[index].effect->editor(oracle.window);return true;};
    oracle.assistantPlan=oracle.assistantResolve(makeAssistantPlan(25,65));
}
OracleRack::~OracleRack(){scanCancel=true;if(scanner.joinable())scanner.join();editing=true;while(readers.load())std::this_thread::yield();active=nullptr;if(oracle.window)DestroyWindow(oracle.window);owned.clear();}
void OracleRack::collect(){if(readers.load(std::memory_order_seq_cst))return;auto*current=active.load();owned.erase(std::remove_if(owned.begin(),owned.end(),[&](auto&p){return p.get()!=current;}),owned.end());}
void OracleRack::publish(std::unique_ptr<Chain>c){c->dry=StereoDelay(c->latency);auto*raw=c.get();owned.push_back(std::move(c));active.store(raw,std::memory_order_seq_cst);int old=latency.exchange(raw->latency);if(old!=raw->latency)pendingLatency=true;collect();}
bool OracleRack::apply(const AssistantPlan&plan){
    if(plan.steps.empty()||plan.steps.size()>16)return false;try{auto next=std::make_unique<Chain>();int external=0,mapped=0;auto built=buildOracleChain(plan,rate,oracle.assistantCapture.get(),[&](size_t done,size_t total){oracle.assistantStatus=L"Calibrating effect "+std::to_wstring(done)+L" / "+std::to_wstring(total);if(oracle.window){InvalidateRect(oracle.window,nullptr,FALSE);UpdateWindow(oracle.window);}},transportTempo,transportBeat);for(auto&stage:built.stages){next->latency+=stage.effect->latency();if(next->latency>1048576)throw std::runtime_error("Oracle chain latency exceeds the limit");next->stages.push_back({stage.step,stage.effect,{},{}});if(!stage.step.path.empty()){external++;mapped+=stage.mapped;}}oracle.assistantReport=built.report;
        publish(std::move(next));oracle.assistantPlan=plan;for(size_t i=0;i<built.stages.size();i++)oracle.assistantPlan.steps[i]=built.stages[i].step;oracle.assistantApplied=true;oracle.assistantStatus=L"Rack applied · "+std::to_wstring(external)+L" user effects · "+std::to_wstring(mapped)+L" mapped controls";if(dirty)dirty();poll();return true;
    }catch(const std::exception&e){oracle.assistantStatus=L"Rack kept unchanged: "+wide(e.what());return false;}
}
bool OracleRack::prepare(double sampleRate){
    if(!std::isfinite(sampleRate)||sampleRate<8000||sampleRate>384000)return false;editing.store(true,std::memory_order_seq_cst);while(readers.load(std::memory_order_seq_cst))std::this_thread::yield();bool ok=true;try{rate=sampleRate;oracle.prepare(sampleRate);auto*c=chain();if(c){c->latency=0;for(auto&s:c->stages)if(s.effect){if(!s.effect->prepare(sampleRate)){s.error="Effect failed to prepare";ok=false;}else s.error.clear();if(!s.step.bypass&&s.error.empty())c->latency+=s.effect->latency();}c->dry=StereoDelay(c->latency);if(latency.exchange(c->latency)!=c->latency)pendingLatency=true;}wet=1;}catch(...){ok=false;}editing.store(false,std::memory_order_seq_cst);return ok;
}
void OracleRack::process(float*l,float*r,int frames,double beat,double tempo,bool playing){
    if(frames<=0||frames>MaxBlock)return;transportTempo=tempo;transportBeat=beat;readers.fetch_add(1,std::memory_order_seq_cst);if(editing.load(std::memory_order_seq_cst)){readers.fetch_sub(1,std::memory_order_seq_cst);return;}auto*c=active.load(std::memory_order_seq_cst);
    if(oracle.assistantCapture)oracle.assistantCapture->process(l,r,frames);
    if(c){for(int n=0;n<frames;n++){dryL[n]=l[n];dryR[n]=r[n];c->dry.process(dryL[n],dryR[n]);}for(auto&s:c->stages)if(s.effect&&s.error.empty()&&!s.step.bypass)s.effect->process(l,r,frames,nullptr,0,beat,tempo,playing);double target=bypass.load()?0:1;for(int n=0;n<frames;n++){wet+=clamp(target-wet,-1./128,1./128);l[n]=(float)(dryL[n]*(1-wet)+l[n]*wet);r[n]=(float)(dryR[n]*(1-wet)+r[n]*wet);if(!std::isfinite(l[n]))l[n]=0;if(!std::isfinite(r[n]))r[n]=0;}}
    readers.fetch_sub(1,std::memory_order_seq_cst);
}
Json OracleRack::state(){
    editing.store(true,std::memory_order_seq_cst);while(readers.load(std::memory_order_seq_cst))std::this_thread::yield();Json j;try{j={{"format","Karsei.OracleRack"},{"version",1},{"oracle",oracle.state()},{"bypass",bypass.load()},{"stages",Json::array()}};if(auto*c=chain())for(auto&s:c->stages){Json item=oracleStepJson(s.step);item["state"]=s.effect?s.effect->state():s.missingState;item["error"]=s.error;j["stages"].push_back(std::move(item));}}catch(...){editing=false;throw;}editing.store(false,std::memory_order_seq_cst);return j;
}
bool OracleRack::restore(const Json&j){
    try{if(!j.is_object()||j.value("format","")!="Karsei.OracleRack"||j.value("version",0)!=1||!j.contains("stages")||!j["stages"].is_array()||j["stages"].size()>16)return false;auto next=std::make_unique<Chain>();std::wstring missing;AssistantPlan plan;
        for(auto&item:j["stages"]){Stage s;s.step=oracleStepFromJson(item);s.missingState=item.value("state",Json());try{s.effect=loadOracleEffect(s.step,rate);if(item.contains("state"))s.effect->restore(item["state"]);if(!s.effect->prepare(rate))throw std::runtime_error("Effect cannot prepare");next->latency+=s.effect->latency();}catch(const std::exception&e){s.effect.reset();s.error=e.what();if(!missing.empty())missing+=L", ";missing+=wide(s.step.name.empty()?s.step.path:s.step.name);}plan.steps.push_back(s.step);next->stages.push_back(std::move(s));}
        if(next->latency>1048576)return false;oracle.restore(j.value("oracle",Json::object()));oracle.assistantStandalone=oracle.assistantAnyPlugins=true;bypass=j.value("bypass",false);publish(std::move(next));oracle.assistantPlan=plan;oracle.assistantApplied=!plan.steps.empty();oracle.assistantStatus=missing.empty()?L"Saved rack restored":L"Missing / unavailable effects bypassed: "+missing;if(oracle.window){SendMessageW(oracle.assistantCombo,CB_SETCURSEL,(int)oracle.spec().parameters[0].value(oracle.values[0]),0);InvalidateRect(oracle.window,nullptr,FALSE);}return true;
    }catch(const std::exception&e){oracle.assistantStatus=wide(e.what());return false;}
}
void OracleRack::readLibrary(){
    auto j=readOracleJson(data/L"library.json");if(j.is_object()){catalog=j.value("catalog",Json::array());if(!catalog.is_array())catalog=Json::array();auto paths=j.value("roots",Json::array());if(paths.is_array())for(auto&p:paths)if(p.is_string())roots.emplace_back(wide(p.get<std::string>()));}if(roots.empty()){AppPaths paths;for(auto&p:paths.pluginDirectories())roots.push_back(p);for(auto&p:discoverPluginFolders())roots.push_back(p.path);}
}
void OracleRack::saveLibrary(){try{fs::create_directories(data);Json paths=Json::array();for(auto&p:roots)paths.push_back(utf8(p.wstring()));auto temp=data/(L"library-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring((uintptr_t)this)+L".tmp");{std::ofstream out(temp);out<<Json{{"roots",paths},{"catalog",catalog}}.dump(2);out.flush();if(!out)throw std::runtime_error("Cannot save Oracle library");}if(!MoveFileExW(temp.c_str(),(data/L"library.json").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace Oracle library");}catch(const std::exception&e){oracle.assistantStatus=wide(e.what());}}
void OracleRack::scan(){
    if(scanRunning)return;if(scanner.joinable())scanner.join();auto helper=moduleDirectory/L"OracleScan.exe";if(!fs::exists(helper)){oracle.assistantStatus=L"OracleScan.exe is missing beside the Oracle plugin";return;}scanCancel=false;scanRunning=true;scanCount=scanTotal=0;scanMessage=L"Finding plugins";auto searchRoots=roots;
    scanner=std::thread([this,helper,searchRoots]{Json result=Json::array();try{auto files=findPlugins(searchRoots);scanTotal=(int)files.size();fs::create_directories(data/L"scan");size_t index=0;for(auto&path:files){if(scanCancel)break;if(oracleKey(utf8(path.stem().wstring())).find("oraclerack")!=std::string::npos){scanCount++;continue;}auto output=data/L"scan"/(std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring((uintptr_t)this)+L"-"+std::to_wstring(index++)+L".json");std::wstring command=quote(helper.wstring())+L" "+quote(path.wstring())+L" "+quote(output.wstring());STARTUPINFOW startup{sizeof startup};startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;PROCESS_INFORMATION process{};
            if(CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,moduleDirectory.c_str(),&startup,&process)){auto start=GetTickCount64();DWORD status=WAIT_TIMEOUT;while(!scanCancel&&(status=WaitForSingleObject(process.hProcess,100))==WAIT_TIMEOUT&&GetTickCount64()-start<15000){}if(status==WAIT_TIMEOUT){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,1000);}CloseHandle(process.hThread);CloseHandle(process.hProcess);auto entry=readOracleJson(output);if(entry.is_object()&&entry.value("ok",false))result.push_back(std::move(entry));std::error_code ec;fs::remove(output,ec);}scanCount++;}}
        catch(...){} {std::lock_guard<std::mutex>lock(scanMutex);scanResults=std::move(result);scanReady=true;}scanRunning=false;});
}
void OracleRack::poll(){
    {std::lock_guard<std::mutex>lock(scanMutex);if(scanReady){catalog=std::move(scanResults);scanReady=false;saveLibrary();oracle.assistantPlan=oracle.assistantResolve(makeAssistantPlan((int)oracle.spec().parameters[0].value(oracle.values[0]),oracle.spec().parameters[1].value(oracle.values[1]),oracle.assistantPlan.analyzed?&oracle.assistantPlan.analysis:nullptr));oracle.assistantApplied=false;oracle.assistantStatus=L"Plugin scan ready · "+std::to_wstring(catalog.size())+L" plugins";}}
    if(scanRunning)oracle.assistantStatus=L"Scanning plugins · "+std::to_wstring(scanCount.load())+L" / "+std::to_wstring(scanTotal.load());
    if(oracle.edited.exchange(false)&&dirty)dirty();
    auto*c=chain();if(c){bool restart=false;for(auto&s:c->stages)if(s.effect){restart|=s.effect->needsRestart();if(s.effect->takeChanges()&&dirty)dirty();}if(restart)prepare(rate);}
    if(pendingLatency&&latencyChanged){pendingLatency=false;latencyChanged();}collect();
}
void OracleRack::libraryMenu(){
    HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,ui::translate(scanRunning?L"Cancel scan":L"Scan all plugin folders").c_str());AppendMenuW(menu,MF_STRING,2,ui::translate(L"Add plugin folder…").c_str());AppendMenuW(menu,MF_STRING,3,ui::translate(L"Add one plugin file…").c_str());if(!roots.empty()){HMENU remove=CreatePopupMenu();for(size_t i=0;i<roots.size();i++)AppendMenuW(remove,MF_STRING,100+(UINT_PTR)i,roots[i].c_str());AppendMenuW(menu,MF_POPUP,(UINT_PTR)remove,ui::translate(L"Remove folder").c_str());}AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,4,ui::language?L"Language: English":L"Язык: Русский");POINT pt;GetCursorPos(&pt);int id=TrackPopupMenu(menu,TPM_RETURNCMD,pt.x,pt.y,0,oracle.window,nullptr);DestroyMenu(menu);
    if(id==1){if(scanRunning)scanCancel=true;else scan();}if(id==2){IFileDialog*dialog=nullptr;if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))){DWORD options=0;dialog->GetOptions(&options);dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR);if(SUCCEEDED(dialog->Show(oracle.window))){IShellItem*item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){roots.emplace_back(path);CoTaskMemFree(path);}item->Release();}}dialog->Release();saveLibrary();}}
    if(id==3){wchar_t file[32768]{};OPENFILENAMEW ofn{sizeof ofn};ofn.hwndOwner=oracle.window;ofn.lpstrFile=file;ofn.nMaxFile=32768;ofn.lpstrFilter=L"VST3 / VST2 effects\0*.vst3;*.dll\0\0";ofn.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;if(GetOpenFileNameW(&ofn)){roots.push_back(fs::path(file).parent_path());saveLibrary();scan();}}
    if(id==4){ui::language=1-ui::language;if(oracle.window)InvalidateRect(oracle.window,nullptr,TRUE);}if(id>=100&&id<100+(int)roots.size()&&!scanRunning){roots.erase(roots.begin()+id-100);saveLibrary();}
}
}
