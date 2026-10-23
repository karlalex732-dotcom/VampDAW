    bool playlistStretch=false;
    PlaylistClipEdit playlistClipEdit=PlaylistClipEdit::None;
    std::tuple<int,int,PlaylistClipEdit>playlistHover{-1,-1,PlaylistClipEdit::None};
    struct PlaylistClipHit {int track=-1,index=-1;PlaylistClipEdit edit=PlaylistClipEdit::None;Rect rect;};
    Rect playlistClipRect(int track,const Clip&c)const{int row=playlistRowIndex(track);if(row<0)return{};float unit=arrangement.w/(float)(visibleBars*4);return{arrangement.x+(float)(c.beat-viewStart)*unit+1,arrangement.y+playlistLayout.y(row,playlistRowOffset)+3,std::max(1.f,(float)c.length*unit-2),std::max(1.f,playlistLayout.height(row)-6)};}
    float playlistFadeX(const Clip&c,Rect r,bool in)const{auto clock=songClock(project);double duration=clock.duration(c.beat,c.beat+c.length),second=in?c.fadeIn:duration-c.fadeOut;double beat=clock.beat(clock.seconds(c.beat)+second);float x=r.x+(float)((beat-c.beat)/c.length)*r.w;return(float)clamp(x,r.x+11,r.x+r.w-11);}
    PlaylistClipHit playlistClipHit(float x,float y)const{
        PlaylistClipHit hit;if(!arrangement.contains(x,y))return hit;int track=playlistTrackAtY(y);if(track<0||track>=(int)project.tracks.size())return hit;auto&t=project.tracks[track];
        for(int i=(int)t.clips.size()-1;i>=0;i--){auto&c=t.clips[i];auto r=playlistClipRect(track,c);if(!r.contains(x,y))continue;hit={track,i,PlaylistClipEdit::None,r};bool audio=(bool)clipAudio(t,c);
            if(audio&&r.w>=40&&r.h>=16&&std::abs(y-(r.y+6))<=6){float a=playlistFadeX(c,r,true),b=playlistFadeX(c,r,false);if(std::abs(x-a)<=6||std::abs(x-b)<=6){hit.edit=std::abs(x-a)<=std::abs(x-b)?PlaylistClipEdit::FadeIn:PlaylistClipEdit::FadeOut;return hit;}}
            float edge=std::min(5.f,r.w*.2f);if(audio&&x<=r.x+edge)hit.edit=PlaylistClipEdit::TrimStart;else if(x>=r.x+r.w-edge)hit.edit=audio&&playlistStretch?PlaylistClipEdit::StretchEnd:PlaylistClipEdit::TrimEnd;else if(y<=r.y+std::min(18.f,r.h))hit.edit=PlaylistClipEdit::Move;return hit;
        }return hit;
    }
    bool playlistClipOwnsPointer(float x,float y)const{auto hit=playlistClipHit(x,y);return hit.edit!=PlaylistClipEdit::None;}
    void updatePlaylistHover(float x,float y){auto hit=editorVisible&&mode==0?playlistClipHit(x,y):PlaylistClipHit{};auto key=std::make_tuple(hit.track,hit.index,hit.edit);if(key!=playlistHover){playlistHover=key;InvalidateRect(window,nullptr,FALSE);}}
    bool playlistClipCursor(float x,float y){if(!editorVisible||mode!=0||tool>1||settingsOpen||exportSettingsOpen||openMenu>=0)return false;auto h=playlistClipHit(x,y);if(h.track<0||project.tracks[h.track].frozen)return false;auto id=h.edit==PlaylistClipEdit::Move?IDC_HAND:(h.edit==PlaylistClipEdit::FadeIn||h.edit==PlaylistClipEdit::FadeOut)?IDC_CROSS:(h.edit==PlaylistClipEdit::TrimStart||h.edit==PlaylistClipEdit::TrimEnd||h.edit==PlaylistClipEdit::StretchEnd)?IDC_SIZEWE:IDC_ARROW;SetCursor(LoadCursorW(nullptr,id));return true;}
    Clip playlistClipPreview(const Clip&c)const{
        if(std::abs(clipGestureDelta)<1e-9)return c;
        auto clock=songClock(project);double minimum=snapping&&!(GetKeyState(VK_MENU)&0x8000)?std::min(snapStep,c.length):.01;
        if(playlistClipEdit==PlaylistClipEdit::TrimStart)return trimAudioStart(c,c.beat+clipGestureDelta,clock,minimum);
        auto audio=clipAudio(project.tracks[drag.index],c);
        if(playlistClipEdit==PlaylistClipEdit::TrimEnd){if(audio)return trimAudioEnd(c,c.beat+c.length+clipGestureDelta,*audio,clock,minimum);Clip out=c;out.length=clamp(c.length+clipGestureDelta,minimum,16384-c.beat);return out;}
        if(playlistClipEdit==PlaylistClipEdit::StretchEnd&&audio)return stretchAudioEnd(c,c.beat+c.length+clipGestureDelta,*audio,clock,minimum);
        Clip out=c;if(playlistClipEdit==PlaylistClipEdit::FadeIn)out.fadeIn=clamp(oldValue+clipGestureDelta,0,clock.duration(c.beat,c.beat+c.length));if(playlistClipEdit==PlaylistClipEdit::FadeOut)out.fadeOut=clamp(oldValue-clipGestureDelta,0,clock.duration(c.beat,c.beat+c.length));return out;
    }
    void drawPlaylistClipChrome(Painter&p,const Clip&c,Rect r,Color color,const std::string&label,bool audio,bool chosen){
        bool hover=hoverMouseInside&&r.contains(gestureX,gestureY)&&arrangement.contains(gestureX,gestureY);auto hit=hover?playlistClipHit(gestureX,gestureY):PlaylistClipHit{};
        float header=std::min(18.f,r.h);p.fill({r.x,r.y,r.w,header},alpha(bg,hover&&hit.edit==PlaylistClipEdit::Move?145:90),2);p.line(r.x,r.y+header,r.x+r.w,r.y+header,alpha(color,75));
        if(r.w>=30){float pad=audio&&r.w>=40&&r.h>=22?20.f:7.f;p.text(wide(label)+(audio&&c.warp?L"  ↔":L""),{r.x+pad,r.y,r.w-pad-8,header},8,color,true);}
        if(!audio)return;auto clock=songClock(project);float bottom=r.y+r.h-1,top=r.y+std::min(header,r.h*.3f);
        auto fade=[&](bool in){double seconds=in?c.fadeIn:c.fadeOut;if(seconds<=0||bottom<=top)return;double b=in?clock.beat(clock.seconds(c.beat)+seconds):clock.beat(clock.seconds(c.beat+c.length)-seconds);float at=r.x+(float)((b-c.beat)/c.length)*r.w,edge=in?r.x:r.x+r.w;
            PointF points[3]={{edge,top},{edge,bottom},{at,top}};SolidBrush shade(alpha(bg,165));p.g.FillPolygon(&shade,points,3);p.line(edge,bottom,at,top,alpha(textColor,220),1.5f);
        };fade(true);fade(false);
        if((chosen||hover)&&r.w>=40&&r.h>=16){for(bool in:{true,false}){float x=playlistFadeX(c,r,in);bool active=hover&&hit.edit==(in?PlaylistClipEdit::FadeIn:PlaylistClipEdit::FadeOut);p.fill({x-4,r.y+2,8,8},active?textColor:color,2);p.line(x-2,r.y+6,x+2,r.y+6,bg,1);}p.line(r.x+2,r.y+std::min(header+3,r.h*.6f),r.x+2,bottom-2,alpha(color,210),2);p.line(r.x+r.w-2,r.y+std::min(header+3,r.h*.6f),r.x+r.w-2,bottom-2,alpha(color,210),2);}
    }
    void queuePlaylistWarp(int track,int index,const Clip&clip){
        if(warpBusy)throw std::runtime_error("Wait for Warp processing to finish");if(warpWorker.joinable())warpWorker.join();auto source=clipAudio(project.tracks.at(track),clip);if(!source)throw std::runtime_error("This clip has no audio");warpJobs={{track,index,clip,source,{}}};warpRevision=poolRevision;warpResult.reset();warpError.clear();warpCompleted=0;warpDone=false;warpBusy=true;auto clock=songClock(project);
        warpWorker=std::thread([this,clock]{try{warpJobs[0].result=renderWarp(*warpJobs[0].source,warpJobs[0].clip,clock);warpResult=warpJobs[0].result;warpCompleted=1;}catch(const std::exception&e){warpError=e.what();}warpDone=true;});refreshLoading();
    }
