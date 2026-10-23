    std::set<int>mixerSelection;int mixerAnchor=0,mixerSlotOffset=0,mixerFxRows=2;
    std::array<Rect,TrackCount+1>mixerStrips{},mixerFxAreas{};
    std::array<float,TrackCount+1>mixerPeakHold{};
    struct MixerValue{int track;float volume,pan;};std::vector<MixerValue>mixerGesture;

    bool mixerControl(const Hit&h)const{return h.index>=0&&h.index<=MasterChannel&&mixerStrips[h.index].w>0&&mixerStrips[h.index].contains(h.rect.x+h.rect.w/2,h.rect.y+h.rect.h/2);}
    std::set<int>mixerTargets(int index)const{
        if(index!=MasterChannel&&mixerSelection.count(index)){std::set<int>result;for(int i:mixerSelection)if(i>=0&&i<(int)project.tracks.size())result.insert(i);if(!result.empty())return result;}
        return {index};
    }
    void mixerSelect(int index,bool visual=false){
        bool control=GetKeyState(VK_CONTROL)&0x8000,shift=GetKeyState(VK_SHIFT)&0x8000;
        auto group=mixerSelection;int anchor=mixerAnchor;
        if(index==MasterChannel){group={index};anchor=index;}
        else if(shift&&anchor>=0&&anchor<(int)project.tracks.size()){
            if(!control)group.clear();group.erase(MasterChannel);int first=visual?playlistRowIndex(anchor):-1,last=visual?playlistRowIndex(index):-1;
            if(first>=0&&last>=0){for(int i=std::min(first,last);i<=std::max(first,last);i++)if(!projectRows[i].folder)group.insert(projectRows[i].id);}
            else for(int i=std::min(anchor,index);i<=std::max(anchor,index);i++)group.insert(i);
        }else if(control){group.erase(MasterChannel);if(!group.erase(index))group.insert(index);if(group.empty())group.insert(index);anchor=index;}
        else{group={index};anchor=index;}
        int lead=group.count(index)?index:*group.begin();select(lead);mixerSelection=std::move(group);mixerAnchor=anchor;
    }
    void beginMixerGesture(const Hit&h){
        mixerGesture.clear();auto group=mixerControl(h)?mixerTargets(h.index):std::set<int>{h.index};
        for(int i:group){auto&t=channel(i);mixerGesture.push_back({i,t.volume,t.pan});}
    }
    void changeMixerValue(const Hit&h,float value,bool pan){
        if(mixerGesture.empty())beginMixerGesture(h);
        auto owner=std::find_if(mixerGesture.begin(),mixerGesture.end(),[&](auto&v){return v.track==h.index;});
        if(owner==mixerGesture.end())return;
        for(auto&v:mixerGesture){auto&t=channel(v.track);if(pan)t.pan=(float)clamp(v.pan+value-owner->pan,-1,1);else t.volume=(float)clamp(owner->volume>1e-8?v.volume*value/owner->volume:v.volume+value,0,1.5);}
        changed();
    }
    void mixerExact(const Hit&h,bool pan){
        auto&t=channel(h.index);double initial=pan?t.pan*100:t.volume>0?20*log10(t.volume):-96;
        auto input=TextPrompt::show(window,pan?L"Pan / -100…100%":L"Volume / -96…+3.52 dB, -inf",number(initial,pan?1:2));if(!input)return;
        try{auto text=*input;std::replace(text.begin(),text.end(),L',',L'.');size_t used=0;double value=std::stod(text,&used);
            while(used<text.size()&&iswspace(text[used]))used++;
            if(used!=text.size()||(pan?(!std::isfinite(value)||value< -100||value>100):(!std::isfinite(value)&&value!=-INFINITY)||value>20*log10(1.5)+.00001))throw std::runtime_error("range");
            if(!pan&&std::isfinite(value)&&value< -96)throw std::runtime_error("range");
            beginMixerGesture(h);history();changeMixerValue(h,pan?(float)(value/100):std::isinf(value)?0.f:(float)std::pow(10.,value/20),pan);mixerGesture.clear();
        }catch(...){notify(pan?L"Enter pan from -100 to 100%":L"Enter volume from -96 to +3.52 dB, or -inf");}
    }
    bool mixerWheel(float x,float y,int delta){
        if(mode!=3||!editorVisible||maximizedPane==5||maximizedPane==6)return false;
        for(auto&h:hits)if((h.action==Volume||h.action==Pan)&&h.rect.contains(x,y)&&mixerControl(h)){
            beginMixerGesture(h);history();auto&t=channel(h.index);double step=(GetKeyState(VK_CONTROL)&0x8000)?.1:.5;
            float value=h.action==Pan?(float)clamp(t.pan+delta/double(WHEEL_DELTA)*step/50,-1,1):(float)clamp((t.volume>0?t.volume:.00001585)*std::pow(10.,delta/double(WHEEL_DELTA)*step/20),0,1.5);
            changeMixerValue(h,value,h.action==Pan);mixerGesture.clear();return true;
        }
        for(auto&r:mixerFxAreas)if(r.w>0&&r.contains(x,y)){mixerSlotOffset=(int)clamp(mixerSlotOffset+(delta>0?-1:1),0,16-mixerFxRows);InvalidateRect(window,nullptr,FALSE);return true;}
        return false;
    }
    void mixerSwitch(const Hit&h,bool solo){
        bool value=solo?!channel(h.index).solo:!channel(h.index).mute;
        history();auto group=mixerControl(h)?mixerTargets(h.index):std::set<int>{h.index};
        for(int i:group)if(solo){if(i!=MasterChannel)channel(i).solo=value;}else channel(i).mute=value;changed();
    }
    void mixerGroupMenu(){
        HMENU menu=CreatePopupMenu();
        const wchar_t*labels[]={L"Select all channels",L"Select current channel only",L"Reset volume to 0 dB",L"Center pan",L"Mute selected channels",L"Unmute selected channels",L"Solo selected channels",L"Clear all solos",L"Route selected channels…",L"Reset all peak meters",L"Group / move selected tracks to folder…"};
        for(int i=0;i<11;i++)AppendMenuW(menu,MF_STRING,i+1,labels[i]);POINT pt;GetCursorPos(&pt);int id=TrackPopupMenu(menu,TPM_RETURNCMD,pt.x,pt.y,0,window,nullptr);DestroyMenu(menu);if(!id)return;
        if(id==11){groupTracksMenu();return;}if(id==1){mixerSelection.clear();for(int i=0;i<(int)project.tracks.size();i++)mixerSelection.insert(i);if(selected==MasterChannel){select(0);mixerSelection.clear();for(int i=0;i<(int)project.tracks.size();i++)mixerSelection.insert(i);}}
        else if(id==2)mixerSelection={selected};
        else if(id==10)mixerPeakHold.fill(0);
        else if(id==9){int dest=chooseDestination(selected);if(dest>=0){auto candidate=project;for(int i:mixerTargets(selected))if(i!=MasterChannel)candidate.tracks[i].output=dest;applyRouting(candidate);}}
        else{history();auto group=mixerTargets(selected);if(id==7||id==8)for(auto&t:project.tracks)t.solo=false;
            for(int i:group){auto&t=channel(i);if(id==3)t.volume=1;if(id==4)t.pan=0;if(id==5)t.mute=true;if(id==6)t.mute=false;if(id==7&&i!=MasterChannel)t.solo=true;}changed();}
        InvalidateRect(window,nullptr,FALSE);
    }
