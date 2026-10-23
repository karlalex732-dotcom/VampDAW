    void mixerButton(Painter&p,const std::wstring&label,Rect r,Action action,bool active=false,int index=0,Color accent=crimson){p.fill(r,active?alpha(accent,45):raised,5);p.text(label,r,r.w<36?8:10,active?accent:textColor,active,1,label!=L"MON"&&label!=L"REC"&&label!=L"IN ▾"&&label!=L"SC ▾");addHit(action,r,index);}
    void mixer(Painter&p,bool mini){
        if(mixerSelection.empty())mixerSelection.insert(selected);
        int linkCount=(int)sidechainLinks().size();float routeHeight=linkCount?44.f+33*std::min(3,linkCount):58.f;
        float top=mini?height-mixerDockHeight-45:work.y+65,bottom=mini?height-60:height-67-routeHeight,stripWidth=0;int columns=std::max(9,std::min(40,(int)(work.w/68)));mixerPageTracks=columns-1;stripWidth=work.w/columns;trackOffset=std::min(trackOffset,std::max(0,(int)project.tracks.size()-mixerPageTracks));
        int fxRows=mini?0:bottom-top>=480?4:bottom-top>=390?2:1;
        if(!mini){
            mixerFxRows=fxRows;mixerSlotOffset=(int)clamp(mixerSlotOffset,0,16-fxRows);
            p.text(L"MIXER",{work.x,work.y,115,27},14,textColor,true);
            p.text(number(project.tracks.size())+L" channels + MASTER",{work.x+120,work.y,std::max(20.f,work.w-210),27},11,muted);
            mixerButton(p,L"‹",{work.x+work.w-76,work.y+1,33,25},TrackPrev);mixerButton(p,L"›",{work.x+work.w-35,work.y+1,33,25},TrackNext);
            mixerButton(p,L"Selected: "+number(mixerSelection.size())+L" ▾",{work.x,work.y+32,145,25},MixerGroup);
            mixerButton(p,L"‹ FX",{work.x+153,work.y+32,50,25},MixerFxPage,false,-1);
            p.text(number(mixerSlotOffset+1)+L"–"+number(mixerSlotOffset+fxRows)+L" / 16",{work.x+208,work.y+32,75,25},10,muted,false,1);
            mixerButton(p,L"FX ›",{work.x+288,work.y+32,50,25},MixerFxPage,false,1);
            mixerButton(p,L"Reset peaks",{work.x+346,work.y+32,110,25},MixerPeak,false,-1);
            p.text(L"Ctrl / Shift: select channels",{work.x+464,work.y+32,std::max(1.f,work.w-464),25},9,muted,false,2);
        }
        for(int column=0;column<columns;column++){
            bool master=column==mixerPageTracks;int index=master?MasterChannel:trackOffset+column;if(!master&&index>=(int)project.tracks.size())continue;
            auto&t=channel(index);Color accent=trackColor(index);float x=work.x+column*stripWidth;
            Rect strip{x+2,top,stripWidth-4,bottom-top};mixerStrips[index]=strip;
            bool chosen=mixerSelection.count(index);p.fill(strip,chosen?raised:master?Color(255,39,25,35):panel,5);p.fill({strip.x,top,strip.w,chosen?3.f:2.f},chosen?crimson:accent,1);
            p.text(master?L"MASTER":number(index+1)+(t.frozen?L" ❄ ":L" / ")+wide(t.name),{x+7,top+8,stripWidth-14,23},mini?9:10,chosen||master?textColor:muted,true,1);addHit(Select,{x,top,stripWidth,31},index);
            float y=top+(mini?37:48),faderBottom=bottom-(mini?30:98),faderTop=mini?y:top+124+fxRows*26+56,faderHeight=std::max(24.f,faderBottom-faderTop);
            if(!mini){
                if(!master){
                    bool instrument=!plugins[index].empty()&&(plugins[index][0]->instrument()||plugins[index][0]->araActive());
                    std::wstring source=t.frozen?L"❄ Frozen":t.audioInput?L"Audio Input":instrument?wide(plugins[index][0]->info.name):t.sampler?L"Sampler":L"+ Source";
                    mixerButton(p,source,{x+7,top+37,stripWidth-14,25},MixerSource,t.frozen||instrument||t.sampler||t.audioInput,index,accent);
                    float half=(stripWidth-21)/2;mixerButton(p,L"REC",{x+7,top+67,half,22},ArmAudio,t.armed,index,accent);mixerButton(p,L"MON",{x+14+half,top+67,half,22},MonitorAudio,t.monitor,index,accent);
                    mixerButton(p,L"IN ▾",{x+7,top+94,half,22},MixerInput,t.audioInput,index);mixerButton(p,t.frozen?L"❄ ↶":L"❄",{x+14+half,top+94,half,22},MixerFreeze,t.frozen,index,accent);
                }else p.text(L"FX",{x+7,top+37,stripWidth-14,25},9,accent,true,1);
                y=top+124;mixerFxAreas[index]={x+7,y,stripWidth-14,fxRows*26.f};
                for(int row=0;row<fxRows;row++){
                    int slot=mixerSlotOffset+row;Rect rect{x+8,y+row*26,stripWidth-16,22};bool exists=slot<(int)plugins[index].size();bool bypass=exists&&slot<(int)t.pluginBypass.size()&&t.pluginBypass[slot];
                    p.fill(rect,Color(255,18,17,24),3);
                    if(exists){p.circle(rect.x+7,rect.y+11,2.5f,bypass?muted:accent);addHit(BypassPlugin,{rect.x,rect.y,14,rect.h},index,slot);}
                    float pad=exists?14:3;p.text(number(slot+1)+L" "+(exists?wide(plugins[index][slot]->info.name):L"+ FX"),{rect.x+pad,rect.y,rect.w-pad-2,rect.h},9,bypass?muted:textColor,false,0,false);
                    addHit(PluginSlot,{rect.x+pad,rect.y,rect.w-pad,rect.h},index,slot);
                }
                p.text(master?L"FX":t.external?L"MIDI OUT":t.output==MasterChannel?L"→ MASTER":L"→ "+number(t.output+1),{x+8,y+fxRows*26+4,stripWidth-16,22},9,muted,false,1);
                if(!master){addHit(Route,{x+8,y+fxRows*26+4,stripWidth-16,22},index);mixerButton(p,stripWidth<90?L"SC ▾":L"Sends / SC ▾",{x+7,y+fxRows*26+30,stripWidth-14,22},SendMenu,false,index);}
            }
            float peak=master?engine.masterPeak.load():engine.peaks[index].load();mixerPeakHold[index]=std::max(mixerPeakHold[index],peak);
            float level=(float)clamp((20*log10(std::max(peak,.00001f))+60)/60,0,1),held=(float)clamp((20*log10(std::max(mixerPeakHold[index],.00001f))+60)/60,0,1),meterX=x+stripWidth*.72f;
            p.fill({meterX,faderTop,mini?5.f:9.f,faderHeight},Color(255,13,13,19),2);p.fill({meterX,faderTop+faderHeight*(1-level),mini?5.f:9.f,faderHeight*level},peak>=1?crimson:Color(255,112,172,150),2);
            if(held>0)p.line(meterX-2,faderTop+faderHeight*(1-held),meterX+(mini?7.f:11.f),faderTop+faderHeight*(1-held),mixerPeakHold[index]>=1?crimson:textColor);
            addHit(MixerPeak,{meterX-4,faderTop-3,mini?13.f:19.f,faderHeight+6},index);
            float faderX=x+stripWidth*.38f;p.line(faderX,faderTop,faderX,faderBottom,Color(255,9,9,14),3);float fy=faderBottom-t.volume/1.5f*faderHeight;
            p.fill({faderX-9,fy-4,18,8},master?crimson:textColor,2);p.line(faderX-5,fy,faderX+5,fy,Color(255,71,53,71));addHit(Volume,{faderX-16,faderTop-4,32,faderHeight+8},index,1);
            if(!mini){
                p.text(mixerPeakHold[index]>0?number(20*log10(mixerPeakHold[index]),1)+L" pk":L"−∞ pk",{x+5,bottom-93,stripWidth-10,17},9,mixerPeakHold[index]>=1?crimson:muted,false,1);addHit(MixerPeak,{x+5,bottom-93,stripWidth-10,17},index);
                p.text(t.volume>0?number(20*log10(t.volume),1)+L" dB":L"−∞ dB",{x+5,bottom-73,stripWidth-10,19},10,textColor,false,1);addHit(MixerExactVolume,{x+5,bottom-73,stripWidth-10,19},index);
                float py=bottom-43;p.line(x+12,py,x+stripWidth-12,py,lineColor,3);p.circle(x+12+(stripWidth-24)*(t.pan+1)/2,py,4,accent);addHit(Pan,{x+7,py-8,stripWidth-14,16},index);
            }
            float by=bottom-24;mixerButton(p,L"M",{x+8,by,24,18},Mute,t.mute,index,accent);if(!master)mixerButton(p,L"S",{x+stripWidth-32,by,24,18},Solo,t.solo,index,accent);
        }
        if(!mini)mixerRouting(p,{work.x,height-59-routeHeight,work.w,routeHeight},stripWidth);
    }
