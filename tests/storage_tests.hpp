#include "plugin_state.hpp"
#include "settings_store.hpp"
#include "editing.hpp"
#include <chrono>

static void storageTests(){
    for(size_t size=0;size<270;size++){
        std::vector<uint8_t>bytes(size);for(size_t i=0;i<size;i++)bytes[i]=(uint8_t)(i*37);
        auto encoded=encodePluginStateBytes(bytes);
        check(decodePluginStateBytes(Json::parse(encoded.dump()))==bytes,"all byte values and partial base64 groups survive JSON persistence");
        check(decodePluginStateBytes(Json(bytes))==bytes,"legacy VST state byte arrays remain supported");
    }
    for(auto invalid:{Json::array({0,-1}),Json::array({256}),Json::array({1.5}),Json{{"encoding","base64"},{"size",1},{"data","!!=="}},Json{{"encoding","base64"},{"size",2},{"data","AA=="}},Json{{"encoding","base64"},{"size",1},{"data","=A=="}}}){
        bool rejected=false;try{decodePluginStateBytes(invalid);}catch(...){rejected=true;}check(rejected,"corrupt VST byte data is rejected");
    }
    fs::path legacy=L"work/test-results/large-legacy.vamp",compact=L"work/test-results/large-compact.vamp";
    constexpr size_t stateBytes=8*1024*1024;std::string line(32,' ');line+="123,\n";std::string block;block.reserve(line.size()*16384);for(int i=0;i<16384;i++)block+=line;
    {std::ofstream out(legacy,std::ios::binary|std::ios::trunc);out<<R"({"format":"VampDAW","version":6,"name":"Legacy large VST","bars":16,"bpm":123,"loop":true,"loopStart":4,"loopEnd":12,"tracks":[{"name":"Lead","notes":[[0,1,60,0.8]],"clips":[[0,4]],"plugins":["state-fixture.vst3"],"pluginStates":[{"component":[)";
        for(size_t i=0;i<stateBytes/16384-1;i++)out<<block;
        for(int i=0;i<16383;i++)out<<line;out<<"123],\"controller\":[0,127,255]}]}]}";
        check(bool(out),"large legacy project fixture is written");}
    auto originalSize=fs::file_size(legacy);check(originalSize>256ull*1024*1024,"fixture reproduces the former 256 MB rejection");
    auto start=std::chrono::steady_clock::now();auto p=loadProject(legacy);double readMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    auto bytes=decodePluginStateBytes(p.tracks[0].pluginStates[0]["component"]);check(bytes.size()==stateBytes&&std::all_of(bytes.begin(),bytes.end(),[](auto v){return v==123;}),"project larger than 256 MB restores every VST state byte");
    check(decodePluginStateBytes(p.tracks[0].pluginStates[0]["controller"])==std::vector<uint8_t>({0,127,255}),"legacy controller state is streamed independently");
    check(p.tracks[0].notes.size()==1&&p.tracks[0].notes[0].pitch==60&&p.loopStart==4&&p.loopEnd==12,"notes and arrangement range survive streaming migration");
    saveProject(p,compact);auto compactSize=fs::file_size(compact);check(compactSize<originalSize/10,"new project serialization removes numeric-array and indentation overhead");
    auto restored=loadProject(compact);check(decodePluginStateBytes(restored.tracks[0].pluginStates[0]["component"])==bytes&&restored.bpm==123&&restored.tracks[0].clips[0].length==4,"compact project reopens with identical plugin state and arrangement");
    clearTimelineRange(restored);check(restored.loopStart==0&&restored.loopEnd==0&&restored.loop&&restored.tracks[0].notes.size()==1,"clearing the timeline range leaves transport loop mode and musical content intact");
    for(auto text:{R"({"format":"VampDAW","version":6,"tracks":[{"pluginStates":[{"component":[256]}]}]})",R"({"format":"VampDAW","version":6,"tracks":[{"pluginStates":[{"component":[null]}]}]})",R"({"format":"VampDAW","version":7,"tracks":[)"}){
        fs::path bad=L"work/test-results/storage-invalid.vamp";{std::ofstream f(bad);f<<text;}bool rejected=false;try{loadProject(bad);}catch(const std::runtime_error&){rejected=true;}check(rejected&&fs::file_size(bad)==strlen(text),"invalid project loading does not modify the original file");
    }
    fs::remove(legacy);
    std::ofstream(L"work/test-results/project-storage.json")<<Json({{"passed",true},{"legacy_file_bytes",originalSize},{"compact_file_bytes",compactSize},{"state_bytes",stateBytes},{"load_ms",readMs},{"legacy_and_compact_state_identical",true},{"rmb_range_clear_preserves_content",true}}).dump(2);
}

static void settingsStorageTests(){
    fs::path root=L"work/test-results/settings-"+std::to_wstring(GetTickCount64());fs::create_directories(root);auto path=fs::absolute(root/L"settings.json");
    Json initial={{"language",1},{"theme",0},{"soundRoots",{"D:/Samples","Z:/Offline Library"}},{"pluginRoots",{"D:/Plugins"}}};
    SettingsStore first,second;first.initialize(Json::object());first.save(path,initial);first.initialize(first.load(path));second.initialize(second.load(path));
    auto a=initial;a["soundRoots"].push_back("D:/New Sounds");first.save(path,a);
    auto b=initial;b["language"]=0;second.save(path,b);auto persisted=first.load(path);
    check(persisted["soundRoots"]==a["soundRoots"]&&persisted["language"]==0,"stale window saving another preference cannot discard added user folders");
    b["soundRoots"].push_back(utf8(L"D:/Музыка/Удары"));second.save(path,b);persisted=first.load(path);
    check(persisted["soundRoots"].size()==4,"independent folder additions from two windows merge");
    a["soundRoots"].erase(1);first.save(path,a);b["theme"]=3;second.save(path,b);persisted=first.load(path);
    check(persisted["soundRoots"].size()==3&&persisted["soundRoots"][1]=="D:/New Sounds","explicit folder removal survives an unrelated save in a stale window");
    SettingsStore reopened;auto r=reopened.load(path);check(r==persisted&&!reopened.recovered,"restart restores the full merged folder list");reopened.initialize(r);
    std::swap(r["soundRoots"][0],r["soundRoots"][2]);r["pluginRoots"].push_back("Z:/Disconnected Plugins");reopened.save(path,r);
    check(reopened.load(path)==r,"folder order and unavailable plugin folders persist");
    auto cleaned=cleanFolderList(Json::array({"D:/Samples/","d:\\samples","",false,"D:/Samples/Kicks"}));check(cleaned.size()==2,"folder comparison ignores Windows case and slash differences");
    auto backup=path;backup+=L".bak";Json expectedBackup;{std::ifstream f(backup);f>>expectedBackup;}{std::ofstream f(path);f<<"{ interrupted write";}
    SettingsStore recovered;auto fromBackup=recovered.load(path);check(recovered.recovered&&fromBackup==expectedBackup,"interrupted settings file recovers the previous complete preferences");recovered.initialize(fromBackup);recovered.save(path,fromBackup);check(!recovered.load(path).empty()&&!recovered.recovered,"recovered settings can be saved and reopened normally");
    // Two independent writers exercise the named mutex and atomic replacement.
    auto racePath=fs::absolute(root/L"concurrent.json");SettingsStore seed;seed.initialize(Json::object());seed.save(racePath,initial);std::atomic<bool>failed=false;
    auto write=[&](int n){try{SettingsStore store;auto own=store.load(racePath);store.initialize(own);own["soundRoots"].push_back("D:/Concurrent"+std::to_string(n));store.save(racePath,own);}catch(...){failed=true;}};
    std::thread one(write,1),two(write,2);one.join();two.join();auto concurrent=seed.load(racePath);check(!failed&&concurrent["soundRoots"].size()==4,"concurrent settings writes preserve both folder additions and valid JSON");
    bool failedWrite=false;auto blocked=root/L"blocked";{std::ofstream f(blocked);f<<"file";}try{seed.save(blocked/L"settings.json",initial);}catch(...){failedWrite=true;}check(failedWrite,"settings write failures are reported instead of silently dropping preferences");
    std::ofstream(L"work/test-results/settings-storage.json")<<Json({{"passed",true},{"two_window_merge",true},{"restart",true},{"explicit_removal",true},{"folder_order",true},{"unicode_and_offline_paths",true},{"backup_recovery",true},{"concurrent_writes",true},{"failed_write_reported",true}}).dump(2);
}
