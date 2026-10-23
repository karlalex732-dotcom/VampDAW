    struct AudioImport {fs::path source;int target=0;bool newTrack=true;double beat=0;uint64_t generation=0;};
    std::deque<AudioImport>audioImports;AudioImport activeAudioImport;std::thread audioImportWorker;
    std::atomic<bool>audioImportDone{false};std::shared_ptr<Sample>importedAudio;std::string audioImportError;uint64_t importGeneration=0,importFileId=0;
    void importAudioFile(const fs::path&path,bool newTrack,double beat=0){
        if(audioRecording){notify(L"Finish the recording before importing audio");return;}finishMidiRecording();
        if(audioImports.size()>=32){notify(L"Audio import queue is full; wait for the current files");return;}
        int target=selected==MasterChannel?liveTrack.load():selected;audioImports.push_back({path,target,newTrack,beat,importGeneration});startAudioImport();
    }
    void startAudioImport(){
        if(audioImportWorker.joinable()||audioImports.empty())return;activeAudioImport=audioImports.front();audioImports.pop_front();audioImportDone=false;importedAudio.reset();audioImportError.clear();
        auto source=activeAudioImport.source;auto destination=root/L"projects/assets"/(L"Audio-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(++importFileId)+L".wav");
        audioImportWorker=std::thread([this,source,destination]{CoInitializeEx(nullptr,COINIT_MULTITHREADED);SetThreadPriority(GetCurrentThread(),THREAD_MODE_BACKGROUND_BEGIN);try{
            auto sample=readAudio(source);if(sample->left.empty())throw std::runtime_error("Audio file is empty");fs::create_directories(destination.parent_path());WaveWriter writer(destination,sample->rate);
            for(size_t at=0;at<sample->left.size();at+=MaxBlock)writer.write(sample->left.data()+at,sample->right.data()+at,(int)std::min<size_t>(MaxBlock,sample->left.size()-at));writer.finish();sample->path=destination;importedAudio=std::move(sample);
        }catch(const std::exception&e){audioImportError=e.what();}SetThreadPriority(GetCurrentThread(),THREAD_MODE_BACKGROUND_END);CoUninitialize();audioImportDone.store(true,std::memory_order_release);PostMessageW(window,WM_APP+9,0,0);});
        notify(L"Loading audio in background: "+source.filename().wstring());
    }
    void finishAudioImport(){
        if(!audioImportWorker.joinable()||!audioImportDone.load(std::memory_order_acquire))return;audioImportWorker.join();auto sample=std::move(importedAudio);auto job=activeAudioImport;
        if(!audioImportError.empty())notify(L"Audio import failed: "+wide(audioImportError));
        else if(job.generation!=importGeneration)notify(L"Audio import cancelled because the project changed");
        else if(sample)try{
            int target=job.target;bool occupied=target<0||target>=(int)project.tracks.size();if(!occupied){auto&t=project.tracks[target];occupied=t.frozen||t.sample||!t.notes.empty()||!t.clips.empty()||!plugins[target].empty()||t.audioInput;}
            if(occupied){if(project.tracks.size()>=TrackCount)throw std::runtime_error("No empty track available");target=(int)project.tracks.size();}
            history();if(target==(int)project.tracks.size())appendTrack(project);auto&t=project.tracks[target];t.sample=sample;t.sampler=false;t.chromatic=false;t.slices.clear();t.external=false;t.audioInput=false;t.name=utf8(job.source.stem().wstring());t.notes.clear();t.patterns.clear();t.activePattern=0;
            double length=std::min(16384.,audioBeatLength(project,job.beat,sample->left.size()/double(sample->rate)));t.clips={{job.beat,length}};project.bars=std::max(project.bars,barsForEnd(job.beat+length));select(target);selectedClip=0;audioStart=audioSelectA=audioSelectB=0;audioSpan=sample->left.size()/double(sample->rate);audioAnalysisText.clear();changed();notify(L"Audio imported. Playback continued without restarting.");
        }catch(const std::exception&e){notify(L"Audio import failed: "+wide(e.what()));}
        startAudioImport();
    }
