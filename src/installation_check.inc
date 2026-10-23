        if(argc>2&&std::wstring(argv[1])==L"--self-check") {
            fs::path reportPath=fs::absolute(argv[2]);
            Json report={{"version","0.4.19-alpha"},{"passed",false}};
            try {
                auto paths=AppPaths::current();
                if(argc>4&&std::wstring(argv[3])==L"--data-dir") {
                    paths.data=fs::absolute(argv[4]);paths.state=paths.data/L"Settings";
                }
                paths.prepare();
                report["installed"]=paths.installed;
                report["binary_directory"]=utf8(paths.binaries.wstring());
                report["data_directory"]=utf8(paths.data.wstring());
                report["settings_directory"]=utf8(paths.state.wstring());
                fs::create_directories(reportPath.parent_path());
                auto scratch=paths.data/L"projects"/(L"SelfCheck-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
                if(!fs::create_directory(scratch))throw std::runtime_error("Verification directory already exists");
                Project project;project.name="Installation check";project.tracks.resize(1);project.bars=1;project.bpm=240;project.loop=false;
                project.tracks[0].notes={{0,.5,60,.7f},{1,.5,64,.7f},{2,.5,67,.7f}};project.tracks[0].clips={{0,4}};
                saveProject(project,scratch/L"check.vamp");
                auto loaded=loadProject(scratch/L"check.vamp");
                if(loaded.tracks.size()!=1||loaded.tracks[0].notes.size()!=3)throw std::runtime_error("Project save/load failed");
                auto plan=compile(loaded);report["encoder_path"]=utf8(audioEncoderPath().wstring());report["exports"]=Json::array();
                for(int i=0;i<7;i++) {
                    ExportOptions options;options.format=(AudioFormat)i;options.tailSeconds=0;
                    auto path=scratch/(L"check."+std::wstring(audioExtension(options.format)));
                    renderAudio(*plan,path,options);
                    if(!fs::exists(path)||fs::file_size(path)<32)throw std::runtime_error("Empty export during installation check");
                    report["exports"].push_back({{"format",utf8(audioFormatName(options.format))},{"bytes",fs::file_size(path)}});
                }
                auto settingsProbe=paths.state/(L"check-"+std::to_wstring(GetCurrentProcessId())+L".json");
                {std::ofstream file(settingsProbe);file<<"{\"writable\":true}";if(!file)throw std::runtime_error("Cannot write user settings");}
                {std::ifstream file(settingsProbe);Json value;file>>value;if(!value.value("writable",false))throw std::runtime_error("Settings read failed");}
                fs::remove(settingsProbe);
                // Delete only the exact, newly created verification files.
                for(auto& entry:fs::directory_iterator(scratch))fs::remove(entry.path());fs::remove(scratch);
                report["standard_vst3_directory"]=utf8(paths.pluginDirectories().front().wstring());
                report["project_round_trip"]=true;report["settings_writable"]=true;report["asio_driver_count"]=asioPorts().size();report["passed"]=true;
            } catch(const std::exception& error) {report["error"]=error.what();}
            std::ofstream file(reportPath);file<<report.dump(2);bool ok=report["passed"]==true&&bool(file);file.close();
            LocalFree(argv);CoUninitialize();return ok?0:1;
        }
