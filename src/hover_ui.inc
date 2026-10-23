    bool hoverGlowEnabled=true,hoverMouseInside=false;
    struct HoverTrail {Rect rect;ui::HoverEnvelope envelope;};
    std::array<HoverTrail,4>hoverTrails{};
    int hoverContext=-1;float hoverCanvasWidth=0,hoverCanvasHeight=0;
    void configureHover(){ui::setHoverMotion(hoverGlowEnabled,animationLevel);if(!hoverGlowEnabled||!animationLevel)for(auto&t:hoverTrails)t.envelope.reset();}
    bool hoverAnimating()const{for(auto&t:hoverTrails)if(t.envelope.moving)return true;return false;}
    Rect hoverTarget()const{
        if(!hoverMouseInside||!hoverGlowEnabled||animationLevel==0||exportBusy)return {};
        if(introEnabled&&!introDismissed&&introTime&&GetTickCount64()-introTime<3800)return {};
        // Modal painters replace hits, so an obscured control cannot light up.
        for(auto i=hits.rbegin();i!=hits.rend();++i){auto r=i->rect;
            if(i->action==None||i->action==Select||i->action==PanelSplitter||i->action==NavScrollbar||i->action==PlaylistTrackResize||r.h>88||r.w>800)continue;
            if(r.contains(gestureX,gestureY))return r;
        }
        if(!settingsOpen&&!exportSettingsOpen&&openMenu<0&&editorVisible&&mode==1&&piano.h>0&&pianoRow>0&&gestureX>=work.x+1&&gestureX<piano.x&&gestureY>=piano.y&&gestureY<piano.y+piano.h){
            int row=(int)((gestureY-piano.y)/pianoRow);return{work.x+1,piano.y+row*pianoRow+1,62,std::max(1.f,pianoRow-1)};
        }return {};
    }
    void drawHover(Painter&p){
        if(!hoverGlowEnabled||animationLevel==0){for(auto&t:hoverTrails)t.envelope.reset();return;}
        int context=mode+100*(settingsOpen?settingsTab+1:0)+1000*(openMenu+1)+10000*int(exportSettingsOpen)+20000*int(editorVisible);
        if(context!=hoverContext||width!=hoverCanvasWidth||height!=hoverCanvasHeight){for(auto&t:hoverTrails)t.envelope.reset();hoverContext=context;hoverCanvasWidth=width;hoverCanvasHeight=height;}
        auto target=hoverTarget();auto now=GetTickCount64();int current=-1;
        auto equal=[](Rect a,Rect b){return std::abs(a.x-b.x)<.1f&&std::abs(a.y-b.y)<.1f&&std::abs(a.w-b.w)<.1f&&std::abs(a.h-b.h)<.1f;};
        for(int i=0;i<4;i++){hoverTrails[i].envelope.advance(now);if(target.w>0&&equal(target,hoverTrails[i].rect))current=i;}
        if(target.w>0&&current<0){current=0;for(int i=1;i<4;i++)if(hoverTrails[i].envelope.value<hoverTrails[current].envelope.value)current=i;hoverTrails[current].rect=target;hoverTrails[current].envelope.reset();}
        for(int i=0;i<4;i++){auto&t=hoverTrails[i];t.envelope.aim(i==current,now);if(t.envelope.value>.001f)ui::drawHoverGlow(p.g,t.rect.f(),t.envelope.value,animationLevel,RGB(crimson.GetR(),crimson.GetG(),crimson.GetB()));}
    }
