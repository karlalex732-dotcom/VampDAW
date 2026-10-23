    void prepareAraRender(Project&snapshot,const PluginRack&rack){for(int track=0;track<(int)snapshot.tracks.size();track++)for(auto&plug:rack[track])if(plug->araActive())plug->syncAra(makeAraTrackContext(snapshot,track));}
    void syncAraTracks(bool&restart){
        std::vector<std::pair<std::shared_ptr<Plugin>,AraTrackContext>>updates;
        for(int id=0;id<(int)project.tracks.size();id++)for(auto&plug:plugins[id])if(plug->araActive()){auto context=makeAraTrackContext(project,id);if(plug->araNeedsUpdate(context))updates.emplace_back(plug,std::move(context));}
        if(updates.empty())return;restart=engine.hostWindow&&engine.state==1;if(restart)engine.stop();
        for(auto&update:updates)update.first->syncAra(update.second);
    }
    void pollAra(std::shared_ptr<Plugin>plug,int track){if(!plug->araActive())return;auto request=plug->pollAra();auto clock=songClock(project);
        if(request.position>=0)engine.seek=clamp(clock.beat(request.position),0,project.bars*4.);
        if(request.playing>=0)engine.playing=request.playing!=0;
        if(request.range){history();project.loopStart=clamp(clock.beat(request.cycleStart),0,project.bars*4.);project.loopEnd=clamp(clock.beat(request.cycleStart+request.cycleDuration),project.loopStart,project.bars*4.);changed();}
        if(request.cycle>=0&&project.loop!=(request.cycle!=0)){history();project.loop=request.cycle!=0;changed();}
        if(track==selected&&selectedClip>=0&&selectedClip<(int)project.tracks[track].clips.size())plug->selectAraClip(project.tracks[track].clips[selectedClip].araId);
    }
    void openAraClip(int track,int clip){try{if(frozenSource(track))return;select(track);selectedClip=clip;clipSelection={{track,clip}};makeAraTrackContext(project,track);if(!plugins[track].empty()&&plugins[track][0]->araActive()){plugins[track][0]->selectAraClip(project.tracks[track].clips.at(clip).araId);plugins[track][0]->editor(window);}else openPluginPicker(false,true);}catch(const std::exception&e){error(e.what());}}
    std::optional<AraTrackContext>araContextForNewPlugin(const fs::path&path,bool required=false){
        if(!AraEnabled){if(required)throw std::runtime_error("ARA is not enabled in this build");return{};}
        auto unavailable=[&](const char*why)->std::optional<AraTrackContext>{if(required)throw std::runtime_error(why);return{};};
        if(selected==MasterChannel||channel(selected).sampler||channel(selected).audioInput||channel(selected).external)return unavailable("ARA requires Playlist audio, not a sampler or live input");
        for(auto&plug:plugins[selected])if(plug->instrument())return unavailable("Remove the instrument before adding an ARA editor");
        bool audio=false;for(auto&clip:channel(selected).clips)audio|=bool(clipAudio(channel(selected),clip));if(!audio)return unavailable("Add an audio clip before opening ARA");
        if(nativeEffectIndex(path)>=0||lower(path.extension().wstring())!=L".vst3")return unavailable("ARA needs a VST3 plugin");auto metadata=describePlugin(path);if(!required&&!metadata.value("ara",false))return{};
        for(int i=0;i<(int)project.tracks.size();i++)if(i!=selected){if(project.tracks[i].output==selected)throw std::runtime_error("ARA needs a dedicated audio track, not a receiving bus");for(auto&send:project.tracks[i].sends)if(send.target==selected&&!send.sidechain)throw std::runtime_error("ARA cannot replace the source on a track receiving audio sends");}
        patternPreview=false;return makeAraTrackContext(project,selected);
    }
