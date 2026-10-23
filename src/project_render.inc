        // The batch path uses the same graph, plugins and WAV exporter as the Export window.
        if(argc>3&&std::wstring(argv[1])==L"--plugin-parameters"){
            auto plugin=std::make_shared<Plugin>();std::string error;
            if(!plugin->load(argv[2],48000,error))throw std::runtime_error(error);
            Json report={{"name",plugin->info.name},{"parameters",Json::array()},{"initial_state",plugin->state()}};
            for(auto&parameter:plugin->parameters())report["parameters"].push_back({{"id",parameter.id},{"name",parameter.name}});
            std::ofstream(fs::path(argv[3]),std::ios::binary)<<report.dump(2);
            plugin.reset();LocalFree(argv);CoUninitialize();return 0;
        }
        if(argc>3&&std::wstring(argv[1])==L"--render-project"){
            SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
            auto project=loadProject(argv[2]);PluginRack rack;Json loaded=Json::array();
            auto load=[&](const Track&track,int channel){if(track.frozen)return;
                for(size_t slot=0;slot<track.pluginPaths.size();slot++){
                    auto plugin=std::make_shared<Plugin>();std::string error;fs::path file=wide(track.pluginPaths[slot]);
                    if(file.is_relative())file=fs::path(argv[2]).parent_path()/file;
                    std::optional<AraTrackContext>ara;if(slot<track.pluginStates.size()&&track.pluginStates[slot].is_object()&&track.pluginStates[slot].contains("ara"))ara=makeAraTrackContext(project,channel);if(!plugin->load(file,48000,error,ara?&*ara:nullptr))throw std::runtime_error(track.name+": "+error);
                    if(slot<track.pluginStates.size())plugin->restore(track.pluginStates[slot]);
                    auto restored=plugin->state();loaded.push_back({{"channel",channel},{"slot",slot},{"name",plugin->info.name},{"state_bytes",restored.value("component",Json::array()).size()}});rack[channel].push_back(plugin);
                }
            };
            for(int i=0;i<(int)project.tracks.size();i++)load(project.tracks[i],i);load(project.master,MasterChannel);
            auto plan=compile(project,rack);fs::path output=fs::absolute(argv[3]);fs::create_directories(output.parent_path());
            ExportOptions options;options.normalize=true;options.bits=24;renderWav(*plan,output,options);
            auto rendered=readWav(output);double peak=0,energy=0;bool finite=true;
            for(size_t n=0;n<rendered->left.size();n++){float l=rendered->left[n],r=rendered->right[n];finite&=std::isfinite(l)&&std::isfinite(r);peak=std::max(peak,double(std::max(std::abs(l),std::abs(r))));energy+=double(l)*l+double(r)*r;}
            Json report={{"renderer","VampDAW 0.4.3"},{"project",utf8(fs::absolute(argv[2]).wstring())},{"output",utf8(output.wstring())},{"sample_rate",rendered->rate},{"frames",rendered->left.size()},{"tracks",project.tracks.size()},{"plugins",loaded},{"finite",finite},{"peak",peak},{"rms",std::sqrt(energy/(rendered->left.size()*2))},{"passed",finite&&energy>0}};
            auto reportFile=output;reportFile.replace_extension(L".render.json");std::ofstream(reportFile,std::ios::binary)<<report.dump(2);
            plan.reset();for(auto&chain:rack)chain.clear();LocalFree(argv);CoUninitialize();return report["passed"]==true?0:1;
        }
