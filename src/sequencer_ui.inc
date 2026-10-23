    double sequencerZoom=1,sequencerStepStart=0;int sequencerBars=1;
    static int validSequencerBars(int value){for(int n:{1,2,4,8,16})if(value==n)return n;return 1;}
    int sequencerSteps()const{return sequencerBars*16;}
    float sequencerRowHeight()const{return (float)clamp(44*sequencerZoom,24,88);}
    float sequencerLabelWidth()const{return (float)clamp(131*sequencerZoom,95,std::min(220.f,work.w*.4f));}
    Rect sequencerGrid()const{return {work.x+sequencerLabelWidth(),work.y+84,std::max(32.f,work.w-sequencerLabelWidth()-20),std::max(24.f,height-dockHeight()-work.y-218)};}
    float sequencerStepWidth()const{return std::max(.75f,sequencerGrid().w/sequencerSteps()*(float)std::max(1.,sequencerZoom));}
    double sequencerVisibleSteps()const{return std::min(double(sequencerSteps()),double(sequencerGrid().w/sequencerStepWidth()));}
    int sequencerRows()const{return std::max(1,(int)(sequencerGrid().h/sequencerRowHeight()));}
    void setSequencerZoom(double value){sequencerZoom=clamp(value,.5,2.);sequencerStepStart=clamp(sequencerStepStart,0,std::max(0.,sequencerSteps()-sequencerVisibleSteps()));trackOffset=(int)clamp(trackOffset,0,std::max(0,(int)project.tracks.size()-sequencerRows()));saveSettings();InvalidateRect(window,nullptr,FALSE);}
    void setSequencerBars(int value){sequencerBars=validSequencerBars(value);sequencerPage=(int)clamp(sequencerPage,0,64-sequencerBars);sequencerStepStart=0;saveSettings();InvalidateRect(window,nullptr,FALSE);}
    void sequencerBarsMenu(){HMENU menu=CreatePopupMenu();for(int n:{1,2,4,8,16})AppendMenuW(menu,MF_STRING|(sequencerBars==n?MF_CHECKED:0),n,(L"Bars: "+number(n)).c_str());POINT point;GetCursorPos(&point);int value=TrackPopupMenu(menu,TPM_RETURNCMD,point.x,point.y,0,window,nullptr);DestroyMenu(menu);if(value)setSequencerBars(value);}
    void sequencer(Painter&p){
        sequencerPage=(int)clamp(sequencerPage,0,64-sequencerBars);p.text(L"SEQUENCER",{work.x,work.y,112,25},12,textColor,true);button(p,L"Bars: "+number(sequencerBars)+L" ▾",{work.x+120,work.y+1,100,25},SequencerBars);
        button(p,L"‹ bar",{work.x+work.w-192,work.y+1,63,25},SequencerPagePrev);p.mono(number(sequencerPage+1),{work.x+work.w-122,work.y+1,43,25},12,textColor);button(p,L"bar ›",{work.x+work.w-73,work.y+1,73,25},SequencerPageNext);
        button(p,L"−",{work.x,work.y+31,29,25},SequencerZoom,false,-1);button(p,number(sequencerZoom*100)+L"%",{work.x+33,work.y+31,63,25},SequencerZoom,false,0);button(p,L"+",{work.x+100,work.y+31,29,25},SequencerZoom,false,1);
        button(p,L"Drum generator",{work.x+141,work.y+31,std::min(175.f,work.w-315),25},OpenGenerator,false,0);
        if(selected!=MasterChannel&&(!channel(selected).sample||channel(selected).sampler)){button(p,L"PAT "+number(channel(selected).activePattern+1)+L" ▾",{work.x+work.w-160,work.y+31,117,25},PatternMenu);button(p,L"+",{work.x+work.w-34,work.y+31,34,25},NewPattern);}
        auto grid=sequencerGrid();float labelWidth=sequencerLabelWidth(),rowHeight=sequencerRowHeight(),stepWidth=sequencerStepWidth(),font=(float)clamp(11*sequencerZoom,9,17);
        sequencerStepStart=clamp(sequencerStepStart,0,std::max(0.,sequencerSteps()-sequencerVisibleSteps()));trackOffset=(int)clamp(trackOffset,0,std::max(0,(int)project.tracks.size()-sequencerRows()));int playStep=engine.playing?(int)std::floor(engine.beat.load()*4)-sequencerPage*16:-1;
        for(int row=0;row<std::min(sequencerRows(),(int)project.tracks.size()-trackOffset);row++){
            int t=trackOffset+row;auto&tr=project.tracks[t];float y=grid.y+row*rowHeight;p.fill({work.x,y,work.w-20,rowHeight-3},t==selected?raised:panel,5);p.fill({work.x+8,y+6,3,rowHeight-15},trackColor(t),1);p.text(wide(tr.name),{work.x+18,y+2,labelWidth-24,rowHeight-7},font,t==selected?textColor:muted,t==selected,0,false);addHit(Select,{work.x,y,labelWidth,rowHeight-3},t);
            // Index notes once per row so a 256-step view does not rescan the pattern for every cell.
            std::array<bool,256>enabled{};for(auto&n:editNotes(tr)){double absolute=n.beat*4;int step=(int)std::llround(absolute)-sequencerPage*16;if(step>=0&&step<sequencerSteps()&&std::abs(absolute-std::round(absolute))<.08)enabled[step]=true;}
            for(int s=0;s<sequencerSteps();s++){
                float left=grid.x+(float)(s-sequencerStepStart)*stepWidth,right=std::min(grid.x+grid.w,left+stepWidth);left=std::max(left,grid.x);if(right-left<.75f)continue;
                float gap=std::min(2.f,(right-left)*.12f);Rect r{left+gap,y+5,right-left-gap*2,rowHeight-13};Color c=trackColor(t);p.fill(r,enabled[s]?c:Color(255,s/4%2?45:36,s/4%2?38:33,s/4%2?52:44),std::min({4.f,r.h/3,r.w/3}));if(s%16==0)p.line(left,y+3,left,y+rowHeight-4,lineColor);if(playStep==s)p.line(r.x,r.y+r.h+2,r.x+r.w,r.y+r.h+2,textColor,2);addHit(None,r,t,100+sequencerPage*16+s);
            }
        }
        if(sequencerBars==1){for(int s=0;s<16;s++){float x=grid.x+(float)(s-sequencerStepStart)*stepWidth;if(x>=grid.x&&x+stepWidth<=grid.x+grid.w+1)p.text(number(s+1),{x,grid.y-22,stepWidth,17},(float)clamp(8*sequencerZoom,8,12),muted,false,1);}}
        else for(int bar=0;bar<sequencerBars;bar++){float x=grid.x+(bar*16-(float)sequencerStepStart)*stepWidth,right=std::min(grid.x+grid.w,x+stepWidth*16);x=std::max(x,grid.x);if(right-x>=14)p.text(number(sequencerPage+bar+1),{x+2,grid.y-22,right-x-2,17},9,textColor,true);}
        p.text(L"Ctrl+wheel: sequencer zoom · Click % to reset",{work.x,height-dockHeight()-94,work.w-20,20},10,muted);miniMixer(p);
    }
