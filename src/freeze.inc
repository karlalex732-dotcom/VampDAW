    bool frozenSource(int index=-1){if(index<0)index=selected;if(index!=MasterChannel&&channel(index).frozen){notify(L"Unfreeze the track before editing its source or effects");return true;}return false;}
    int freezeExportTrack=-1,folderFreezeTotal=0,folderFreezeCompleted=0;bool folderFreezeActive=false,folderFreezeResume=false;std::deque<int>folderFreezePending;std::wstring folderFreezeName;
    void endFolderFreeze(){folderFreezePending.clear();folderFreezeActive=false;exportResume=folderFreezeResume;restoreAfterExport();changed();}
    void freezeFolder(int folder,bool unfreeze){if(exportBusy||sampleBusy)return;auto members=folderMembers(project,folder);if(members.empty()){notify(L"This folder has no tracks");return;}
        if(unfreeze){std::vector<int>ids;for(int id:members)if(project.tracks[id].frozen)ids.push_back(id);if(ids.empty()){notify(L"This folder has no frozen tracks");return;}capturePluginStates();history();for(int id:ids){auto&t=project.tracks[id];t.frozen=false;t.freezeAudio.reset();}syncPlugins();changed();notify(L"Folder unfrozen · "+number(ids.size())+L" tracks");return;}
        try{auto plan=compile(project,plugins);auto ids=folderFreezeOrder(project,folder,plan->order);if(ids.empty()){notify(L"No tracks available to freeze; record external MIDI instruments first");return;}finishMidiRecording();finishAudioRecording();capturePluginStates();history();folderFreezeName=projectFolderPath(folder);folderFreezeTotal=(int)ids.size();folderFreezeCompleted=0;folderFreezePending=std::deque<int>(ids.begin(),ids.end());folderFreezeResume=engine.playing;folderFreezeActive=true;int next=folderFreezePending.front();folderFreezePending.pop_front();toggleFreeze(next);}catch(const std::exception&e){if(folderFreezeActive)endFolderFreeze();error(e.what());}
    }
    void toggleFreeze(int index){
        if(index<0||index>=(int)project.tracks.size()||exportBusy||sampleBusy)return;
        auto&t=project.tracks[index];
        if(t.frozen){history();t.frozen=false;t.freezeAudio.reset();syncPlugins();changed();notify(L"Track unfrozen: instruments, effects and automation restored");return;}
        if(t.external){notify(L"Record external MIDI instruments as audio before freezing");return;}
        finishMidiRecording();finishAudioRecording();
        try{LoadingOperation loading(*this,L"Preparing freeze",wide(t.name));
            exportResume=folderFreezeActive?false:engine.playing.load();engine.stop();engine.stopAudition();capturePluginStates();
            auto snapshot=project;for(auto&track:snapshot.tracks){track.solo=false;track.monitor=false;}snapshot.tracks[index].mute=false;
            PluginRack rack=plugins;rack[MasterChannel].clear();snapshot.master=Track{};
            auto plan=compile(snapshot,rack);plan->tapTrack=index;plan->latency=plan->tracks[index].outputLatency;
            fs::create_directories(root/L"projects/assets/freeze");
            exportPath=root/L"projects/assets/freeze"/(L"freeze-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".wav");
            ExportOptions options;options.rate=(int)engine.rate.load();options.bits=32;options.tailSeconds=2;
            if(exportWorker.joinable())exportWorker.join();exportError.clear();exportDone=false;exportControl.renderWeight=1;exportControl.progress=0;exportControl.phase=0;exportControl.cancel=false;
            freezeExportTrack=index;exportStarted=GetTickCount64();exportBusy=true;exportSettingsOpen=false;
            auto shared=std::shared_ptr<RenderPlan>(plan.release());auto file=exportPath;
            exportWorker=std::thread([this,shared,file,options]{CoInitializeEx(nullptr,COINIT_MULTITHREADED);try{renderWav(*shared,file,options,&exportControl);}catch(const std::exception&e){exportError=e.what();}CoUninitialize();exportDone=true;});
            InvalidateRect(window,nullptr,FALSE);
        }catch(const std::exception&e){freezeExportTrack=-1;exportBusy=false;if(folderFreezeActive)endFolderFreeze();else restoreAfterExport();error(e.what());}
    }
    bool finishFreeze(){
        if(freezeExportTrack<0)return false;int index=freezeExportTrack;freezeExportTrack=-1;
        if(exportError.empty()&&!exportControl.cancel)try{LoadingOperation loading(*this,L"Loading frozen audio",wide(project.tracks.at(index).name));
            auto audio=readWav(exportPath);if(!folderFreezeActive)history();auto&t=project.tracks.at(index);t.freezeAudio=audio;t.freezeBpm=project.bpm;t.frozen=true;
            t.monitor=false;t.armed=false;plugins[index].clear();modified=true;if(folderFreezeActive)folderFreezeCompleted++;
        }catch(const std::exception&e){exportError=e.what();}
        if(folderFreezeActive){if(exportError.empty()&&!exportControl.cancel&&!folderFreezePending.empty()){int next=folderFreezePending.front();folderFreezePending.pop_front();toggleFreeze(next);return true;}
            bool cancelled=exportControl.cancel;auto failure=exportError;int completed=folderFreezeCompleted,total=folderFreezeTotal;endFolderFreeze();if(!failure.empty()&&!cancelled)error(failure);else notify((cancelled?L"Folder freeze cancelled · ":L"Folder frozen · ")+number(completed)+L" / "+number(total)+L" · Undo restores the folder");return true;}
        restoreAfterExport();
        if(!exportError.empty()&&!exportControl.cancel)error(exportError);else notify(exportControl.cancel?L"Freeze cancelled; original track preserved":L"Track frozen. Right-click the track → Unfreeze track");
        InvalidateRect(window,nullptr,FALSE);return true;
    }
