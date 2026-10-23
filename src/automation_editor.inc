    Rect automationPlot{};
    double automationStart=0,automationSpan=32,automationLastBeat=-1;
    int automationPoint=-1;float automationLastValue=0;

    void normalizeAutomationView(){
        automationSpan=clamp(automationSpan,1,std::max(1.,project.bars*4.));
        automationStart=clamp(automationStart,0,std::max(0.,project.bars*4.-automationSpan));
    }
    void automationEditor(Painter&p){
        normalizeAutomationView();auto&t=channel(selected);Color c=trackColor(selected);
        p.text(L"AUTOMATION",{work.x,work.y,200,27},14,textColor,true);
        p.text(wide(t.name),{work.x+205,work.y,work.w-205,27},12,c,true,2);
        float x=work.x,y=work.y+35,w=work.w;
        button(p,L"Channel ▾",{x,y,128,28},AutomationChannel);
        button(p,wide(t.automationName)+L" ▾",{x+136,y,w-458,28},AutoTarget,true,selected);
        button(p,t.automationEnabled?L"ON":L"OFF",{x+w-314,y,56,28},AutoEnable,t.automationEnabled);
        button(p,L"Clear",{x+w-250,y,64,28},ClearAuto);
        button(p,snapping?L"Snap ▾":L"Snap off ▾",{x+w-178,y,94,28},PianoSnap,snapping);
        button(p,L"−",{x+w-76,y,32,28},AutomationZoom,false,-1);
        button(p,L"+",{x+w-38,y,32,28},AutomationZoom,false,1);
        automationPlot={work.x+54,work.y+106,work.w-72,std::max(90.f,height-work.y-192)};
        auto&r=automationPlot;p.fill(r,panel,5);
        for(int row=0;row<=10;row++){
            float yy=r.y+r.h*row/10;
            p.line(r.x,yy,r.x+r.w,yy,row==5?blend(lineColor,c,.3f):lineColor);
            if(row%2==0)p.text(number(100-row*10)+L"%",{work.x,yy-10,47,20},10,muted,false,2);
        }
        double grid=std::max(.25,snapStep);while(grid/automationSpan*r.w<12)grid*=2;
        auto state=p.g.Save();p.g.SetClip(Rect{r.x,r.y-24,r.w,r.h+24}.f());
        for(double beat=std::floor(automationStart/grid)*grid;beat<=automationStart+automationSpan+1e-8;beat+=grid){
            float xx=r.x+(float)((beat-automationStart)/automationSpan)*r.w;
            bool bar=std::abs(beat/4-std::round(beat/4))<1e-7;
            p.line(xx,r.y,xx,r.y+r.h,bar?blend(lineColor,c,.25f):alpha(lineColor,100));
            if(bar)p.text(number(beat/4+1),{xx+4,r.y-24,42,20},10,textColor);
        }
        std::vector<PointF>curve;
        for(float xx=0;xx<=r.w;xx+=2){double beat=automationStart+xx/r.w*automationSpan;curve.emplace_back(r.x+xx,r.y+(1-automationAt(t.automation,beat))*r.h);}
        if(curve.size()>1){Pen pen(t.automationEnabled?c:muted,2);p.g.DrawLines(&pen,curve.data(),(INT)curve.size());}
        for(size_t i=0;i<t.automation.size();i++){
            auto&point=t.automation[i];float xx=r.x+(float)((point.beat-automationStart)/automationSpan)*r.w;
            if(xx>=r.x&&xx<=r.x+r.w){float yy=r.y+(1-point.value)*r.h;p.circle(xx,yy,(int)i==automationPoint?5:3,t.automationEnabled?c:muted);if((int)i==automationPoint)p.circle(xx,yy,2,textColor);}
        }
        double beat=engine.beat.load();
        if(beat>=automationStart&&beat<=automationStart+automationSpan){float xx=r.x+(float)((beat-automationStart)/automationSpan)*r.w;p.line(xx,r.y-15,xx,r.y+r.h,textColor,1.3f);}
        p.g.Restore(state);
        p.text(L"Draw: mouse · Erase: right mouse · Ctrl+wheel: zoom · Shift+wheel / arrows: scroll",{work.x,height-55,work.w,22},9,muted);
    }
    bool automationInput(float x,float y,bool right){
        if(!automationPlot.contains(x,y))return false;
        history();automationLastBeat=-1;drag={None,automationPlot,selected,right?10:9};editGesture=true;SetCapture(window);automationDrawPoint(x,y);return true;
    }
    void automationDrawPoint(float x,float y){
        if(!editGesture||drag.index!=selected)return;auto&t=channel(selected);auto&r=automationPlot;
        double step=snapping&&!(GetKeyState(VK_MENU)&0x8000)?snapStep:automationSpan/std::max(1.f,r.w);
        double beat=clamp(automationStart+(x-r.x)/r.w*automationSpan,0,project.bars*4.);
        if(snapping&&!(GetKeyState(VK_MENU)&0x8000))beat=clamp(std::round(beat/step)*step,0,project.bars*4.);
        float value=(float)clamp(1-(y-r.y)/r.h,0,1);
        double first=automationLastBeat<0?beat:automationLastBeat;float firstValue=automationLastBeat<0?value:automationLastValue;
        if(drag.extra==10){
            double lo=std::min(first,beat)-automationSpan/r.w*8,hi=std::max(first,beat)+automationSpan/r.w*8;
            t.automation.erase(std::remove_if(t.automation.begin(),t.automation.end(),[&](auto&point){return point.beat>=lo&&point.beat<=hi;}),t.automation.end());automationPoint=-1;
        }else{
            int count=(int)std::min(2048.,std::max(1.,std::ceil(std::abs(beat-first)/std::max(.001,step))));
            for(int i=1;i<=count&&t.automation.size()<99999;i++){double fraction=i/double(count);putPoint(t,first+(beat-first)*fraction,firstValue+(value-firstValue)*(float)fraction,std::max(.00001,step*.3));}
            automationPoint=-1;double nearest=1e20;
            for(int i=0;i<(int)t.automation.size();i++)if(std::abs(t.automation[i].beat-beat)<nearest){nearest=std::abs(t.automation[i].beat-beat);automationPoint=i;}
        }
        automationLastBeat=beat;automationLastValue=value;changed();
    }
    bool automationKey(UINT key,bool control,bool alt){
        if(!editorVisible||mode!=7||navigationFocus!=0||alt||(maximizedPane>=0&&maximizedPane!=7))return false;
        if(!control&&(key==VK_LEFT||key==VK_RIGHT)){automationStart+=(key==VK_LEFT?-1:1)*automationSpan*.1;normalizeAutomationView();InvalidateRect(window,nullptr,FALSE);return true;}
        if(key==VK_PRIOR||key==VK_NEXT){automationSpan*=key==VK_PRIOR?.5:2;normalizeAutomationView();InvalidateRect(window,nullptr,FALSE);return true;}
        if(key!=VK_UP&&key!=VK_DOWN&&key!=VK_DELETE)return false;
        auto&t=channel(selected);
        if(automationPoint>=0&&automationPoint<(int)t.automation.size()){
            history();if(key==VK_DELETE){t.automation.erase(t.automation.begin()+automationPoint);automationPoint=-1;}
            else{auto&point=t.automation[automationPoint];point.value=(float)clamp(point.value+(key==VK_UP?1:-1)*(control?.001:.01),0,1);}
            changed();
        }return true;
    }
