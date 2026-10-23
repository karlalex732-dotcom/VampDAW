    void bindAssistant(const std::shared_ptr<Plugin>&plug,int track){
        if(!plug->native||plug->native->kind!=46)return;auto&fx=*plug->native;fx.assistantMaster=track==MasterChannel;if(fx.assistantApply)return;
        if(fx.assistantMaster&&fx.assistantAuto&&fx.values[0].load()==0)fx.set(0,fx.spec().parameters[0].normalized(25));
        fx.assistantResolve=[this,source=std::weak_ptr<Plugin>(plug)](const AssistantPlan&plan){auto owner=source.lock();if(!owner)return plan;auto&f=*owner->native;return resolveOraclePlan(plan,f.assistantAnyPlugins,catalog,f.assistantOverrides);};
        fx.assistantChoose=[this,source=std::weak_ptr<Plugin>(plug)](int row){auto owner=source.lock();if(!owner)return false;auto&f=*owner->native;if(row<0||row>=(int)f.assistantPlan.steps.size())return false;auto&step=f.assistantPlan.steps[row];auto chosen=chooseOracleEffect(f.window,catalog,step);if(!chosen)return false;auto key=std::to_string(step.kind);if(*chosen=="@automatic")f.assistantOverrides.erase(key);else f.assistantOverrides[key]=*chosen;f.edited=true;return true;};
        fx.assistantLibrary=[this]{if(catalogDone)beginCatalog();else notify(L"A plugin scan is already running");};
        fx.assistantApply=[this,source=std::weak_ptr<Plugin>(plug)](const AssistantPlan&plan){
            auto owner=source.lock();if(!owner)return false;int target=-1;for(int channel:channelIds())if(std::find(plugins[channel].begin(),plugins[channel].end(),owner)!=plugins[channel].end()){target=channel;break;}
            if(target<0||channel(target).frozen||recording||audioRecording||exportBusy)return false;
            if(plan.steps.empty()||plugins[target].size()+plan.steps.size()>PluginSlotCount){notify(L"Oracle Assist: not enough free effect slots");return false;}
            std::vector<std::shared_ptr<Plugin>>additions;int external=0,mapped=0;
            try{LoadingOperation loading(*this,L"Calibrating Oracle chain");auto built=buildOracleChain(plan,engine.rate,owner->native->assistantCapture.get(),[&](size_t done,size_t total){loading.update(L"Effect "+number(done)+L" / "+number(total),done/double(std::max<size_t>(1,total)));},songClock(project).bpmAt(engine.beat),engine.beat);owner->native->assistantReport=built.report;for(auto&stage:built.stages){additions.push_back(stage.effect);if(!stage.step.path.empty()){external++;mapped+=stage.mapped;}}for(size_t i=0;i<built.stages.size();i++)owner->native->assistantPlan.steps[i]=built.stages[i].step;}
            catch(const std::exception&e){error(e.what());return false;}
            bool wasPlaying=engine.playing;engine.stop();capturePluginStates();history();auto&t=channel(target);auto&rack=plugins[target];int at=(int)(std::find(rack.begin(),rack.end(),owner)-rack.begin())+1;
            t.pluginStates.resize(rack.size());t.pluginBypass.resize(rack.size(),false);if(t.automationTarget==2&&t.automationSlot>=at)t.automationSlot+=(int)additions.size();for(auto&lane:t.automationLanes)if(lane.target==2&&lane.slot>=at)lane.slot+=(int)additions.size();
            auto applied=std::make_shared<std::vector<std::weak_ptr<Plugin>>>();for(auto&effect:additions){rack.insert(rack.begin()+at,effect);t.pluginPaths.insert(t.pluginPaths.begin()+at,utf8(effect->path.wstring()));t.pluginStates.insert(t.pluginStates.begin()+at,effect->state());t.pluginBypass.insert(t.pluginBypass.begin()+at,false);applied->push_back(effect);at++;}
            owner->native->assistantOpen=[applied,this](int index){if(index<0||index>=(int)applied->size())return false;if(auto effect=(*applied)[index].lock()){effect->editor(window);return true;}return false;};
            slotOffset=(at-1)/4*4;publish();if(engine.hostWindow)engine.start(desiredAudio);engine.playing=wasPlaying;changed();notify(L"Oracle: chain added after Oracle · "+number(external)+L" user effects · "+number(mapped)+L" mapped parameters · Ctrl+Z");return true;
        };
    }
