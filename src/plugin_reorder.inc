    Hit pluginDragSource{};bool pluginDragging=false;ULONGLONG pluginScrollTime=0;
    void swapEffects(int track,int from,int to){
        auto&chain=plugins[track];if(from==to||from<0||to<0||from>=(int)chain.size()||to>=(int)chain.size())return;
        if(frozenSource(track))return;
        if(chain[from]->araActive()||chain[to]->araActive()){notify(L"ARA stays first on the audio track");return;}
        if(chain[from]->instrument()||chain[to]->instrument()){notify(L"VST-инструмент остаётся первым в цепи");return;}
        finishMidiRecording();finishAudioRecording();bool wasPlaying=engine.playing;engine.stop();
        try{capturePluginStates();history();swapPluginSlots(channel(track),from,to);std::swap(chain[from],chain[to]);automationPoint=-1;slotOffset=to/4*4;publish();engine.start(desiredAudio);engine.playing=wasPlaying;changed();}
        catch(const std::exception&e){engine.start(desiredAudio);engine.playing=wasPlaying;error(e.what());}
    }
    void beginPluginDrag(Hit source,float x,float y){
        pluginDragSource=source;pluginDragging=false;dragStartX=x;dragStartY=y;gestureX=x;gestureY=y;pluginScrollTime=GetTickCount64();drag={None,source.rect,source.index,61};editGesture=true;SetCapture(window);
    }
    void finishPluginDrag(){
        bool moved=pluginDragging;auto source=pluginDragSource;Hit target{};
        if(moved)for(auto&h:hits)if(h.action==PluginSlot&&h.index==source.index&&h.rect.contains(gestureX,gestureY))target=h;
        pluginDragging=false;editGesture=false;ReleaseCapture();
        if(moved&&target.action==PluginSlot){int last=(int)plugins[source.index].size()-1;swapEffects(source.index,source.extra,std::min(last,target.extra));}
        else if(!moved&&source.extra<(int)plugins[source.index].size())plugins[source.index][source.extra]->editor(window);
        InvalidateRect(window,nullptr,FALSE);
    }
    void pluginDragScroll(){
        if(!editGesture||drag.extra!=61||!pluginDragging||GetTickCount64()-pluginScrollTime<400)return;
        for(auto&h:hits)if(h.rect.contains(gestureX,gestureY)){
            if(h.action==MixerFxPage){mixerSlotOffset=(int)clamp(mixerSlotOffset+h.index*mixerFxRows,0,16-mixerFxRows);}
            else if(h.action==SlotPrev||h.action==SlotNext){slotOffset=(int)clamp(slotOffset+(h.action==SlotPrev?-4:4),0,12);}
            else continue;pluginScrollTime=GetTickCount64();InvalidateRect(window,nullptr,FALSE);break;
        }
    }
    void pluginDragPreview(Painter&p){
        if(!editGesture||drag.extra!=61||!pluginDragging)return;
        for(auto&h:hits)if(h.action==PluginSlot&&h.index==pluginDragSource.index&&h.rect.contains(gestureX,gestureY)){
            p.line(h.rect.x,h.rect.y,h.rect.x+h.rect.w,h.rect.y,crimson,2);p.line(h.rect.x,h.rect.y+h.rect.h,h.rect.x+h.rect.w,h.rect.y+h.rect.h,crimson,2);
        }
        auto&chain=plugins[pluginDragSource.index];if(pluginDragSource.extra>=(int)chain.size())return;
        Rect tip{std::min(gestureX+12,width-245),std::min(gestureY+15,height-62),230,28};p.fill(tip,raised,5);p.text(L"↕ "+wide(chain[pluginDragSource.extra]->info.name),{tip.x+8,tip.y,tip.w-16,tip.h},11,textColor,true);
    }
