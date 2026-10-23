    PlaylistWaveformCache playlistWaveforms;
    void drawPlaylistWaveform(Painter&p,const std::shared_ptr<Sample>&audio,const Clip&clip,Rect box,float start,float unit,Color color,const TempoMap&clock){
        auto peaks=playlistWaveforms.get(audio,window);float center=box.y+box.h*.5f,amplitude=std::max(1.f,(box.h-4)*.5f);
        float displayScale=peaks&&peaks->peak>1e-9f?1.f/peaks->peak:1.f;
        float left=std::max(box.x,arrangement.x),right=std::min(box.x+box.w,arrangement.x+arrangement.w);
        p.line(left,center,right,center,alpha(color,65));
        for(float x=left;x<right;x+=1){
            double beat=clip.beat+(x-start)/unit,next=clip.beat+(std::min(x+1,right)-start)/unit;
            double offset=clip.warp&&clip.warpAudio?0:clip.offset;
            double first=(offset+clock.duration(clip.beat,beat))*audio->rate,last=(offset+clock.duration(clip.beat,next))*audio->rate;
            if(clip.warp&&!clip.warpAudio){first=warpSourceAt(clip,beat-clip.beat)*audio->rate;last=warpSourceAt(clip,next-clip.beat)*audio->rate;}
            auto range=playlistWaveformRange(*audio,peaks.get(),first,last);if(!range.valid)continue;
            float top=center-(float)clamp(range.high*displayScale,-1,1)*amplitude;
            float bottom=center-(float)clamp(range.low*displayScale,-1,1)*amplitude;
            p.line(x,top,x,bottom,alpha(color,215),1.05f);
        }
    }
