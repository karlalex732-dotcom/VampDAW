#include "engine.hpp"
#include <chrono>
#include <iostream>
using namespace vamp;
int main(int argc,char**argv){try{
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);Project p;p.tracks.resize(32);p.bars=8;PluginRack rack;
    for(int i=0;i<8;i++){auto&t=p.tracks[i];t.notes={{0,4,48+i,.3f},{0,4,60+i,.2f},{2,2,67+i,.2f}};for(int b=0;b<8;b++)t.clips.push_back({b*4.,4});for(auto id:{L"blood-eq",L"fang-compressor"}){auto plug=std::make_shared<Plugin>();std::string error;if(!plug->load(fs::path(L"@vamp")/(std::wstring(id)+L".vampfx"),48000,error))throw std::runtime_error(error);rack[i].push_back(plug);}}
    auto plan=compile(p,rack);auto renderer=std::make_unique<Renderer>();std::array<float,MaxBlock*2>out{};Json rows=Json::array();
    for(int rate:{44100,48000,96000})for(int block:{64,128,512,1024})for(bool playing:{false,true}){
        std::array<double,3>timings{};double energy=0;for(int run=0;run<3;run++){for(auto&chain:rack)for(auto&plugin:chain)if(!plugin->prepare(rate))throw std::runtime_error("prepare");renderer->rate=rate;renderer->reset();renderer->render(*plan,out.data(),block,playing);auto start=std::chrono::steady_clock::now();for(int offset=0;offset<rate;offset+=block){int n=std::min(block,rate-offset);renderer->render(*plan,out.data(),n,playing);for(int i=0;i<n*2;i++)energy+=out[i]*out[i];}timings[run]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
        std::sort(timings.begin(),timings.end());rows.push_back({{"rate",rate},{"block",block},{"playing",playing},{"median_ms_per_audio_second",timings[1]},{"load_percent",timings[1]/10},{"energy",energy}});
    }
    Json report={{"passed",true},{"description","32 channels; eight active channels with Blood EQ + Fang Compressor; median of three one-second renders"},{"rows",rows}};std::ofstream(argc>1?argv[1]:"work/test-results/dsp-benchmark.json")<<report.dump(2);std::cout<<"DSP benchmark completed\n";CoUninitialize();return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
