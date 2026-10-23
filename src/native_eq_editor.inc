namespace {
constexpr COLORREF eqColors[]={RGB(235,74,108),RGB(214,105,168),RGB(168,115,225),RGB(124,147,226),RGB(89,185,183),RGB(154,193,139),RGB(224,180,103)};
const wchar_t*eqTypes[]={L"Bell",L"Low shelf",L"High shelf",L"High-pass",L"Low-pass",L"Band-pass",L"Notch",L"Off"};
struct EqLayout {
    int w,h;RECT graph;int bandY,detailY,outputY;
    EqLayout(int width,int height):w(width),h(height){graph={54,153,w-56,std::max(263,h-181)};bandY=graph.bottom+30;detailY=graph.bottom+65;outputY=graph.bottom+122;}
    double frequency(int x)const{return 20*std::pow(1000.,clamp((x-graph.left)/double(graph.right-graph.left),0,1));}
    int x(double hz)const{return graph.left+(int)(std::log(clamp(hz,20,20000)/20)/std::log(1000.)*(graph.right-graph.left));}
    int y(double db)const{return graph.top+(int)((18-clamp(db,-18,18))/36*(graph.bottom-graph.top));}
    RECT detail(int column)const{int width=(w-44)/4;return{22+column*width,detailY,22+(column+1)*width-7,detailY+49};}
    RECT output(int column)const{int width=(w-44)/2;return{22+column*width,outputY,22+(column+1)*width-7,outputY+26};}
    RECT tab(int band)const{int width=(w-44)/7;return{22+band*width,bandY,22+(band+1)*width-5,bandY+26};}
};
bool inEqRect(RECT r,int x,int y){return x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom;}
double eqValue(const NativeEffect&fx,int id){return fx.spec().parameters[id].value(fx.values[id].load(std::memory_order_relaxed));}
int eqMode(const NativeEffect&fx,int band){return (int)eqValue(fx,23+band);}
int eqPointBand(const NativeEffect&fx,const EqLayout&z,int x,int y,bool nearOnly=false){
    double best=nearOnly?24*24:1e20;int result=-1;
    for(int band=0;band<7;band++){int base=eqBandBase(band),mode=eqMode(fx,band);double px=z.x(eqValue(fx,base)),py=z.y(mode>=3?0:eqValue(fx,base+1));double distance=(x-px)*(x-px)+(y-py)*(y-py);if(distance<best){best=distance;result=band;}}
    return result;
}
int eqParameterAt(const NativeEffect&fx,const EqLayout&z,int x,int y){
    int base=eqBandBase(fx.eqSelected);for(int i=0;i<3;i++)if(inEqRect(z.detail(i),x,y))return base+i;
    if(inEqRect(z.output(0),x,y))return 18;if(inEqRect(z.output(1),x,y))return 19;return -1;
}
void paintEqualizer(NativeEffect&fx,HDC dc,int width,int height){
    EqLayout z(width,height);fillFx(dc,{22,128,width-22,height-35},background);
    textFx(dc,L"7 BANDS / PARAMETRIC",{24,126,width-315,149},mutedInk);
    fillFx(dc,{width-300,126,width-164,149},panelColor);textFx(dc,fx.analyzerEnabled?L"Spectrum: ON":L"Spectrum: OFF",{width-294,126,width-164,149},fx.analyzerEnabled?ink:mutedInk,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
    fillFx(dc,{width-156,126,width-24,149},panelColor);textFx(dc,fx.analyzerPre?L"PRE / input":L"POST / output",{width-153,126,width-24,149},accent,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
    using namespace Gdiplus;
    {
        Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);
        RectF graph((float)z.graph.left,(float)z.graph.top,(float)(z.graph.right-z.graph.left),(float)(z.graph.bottom-z.graph.top));
        LinearGradientBrush backdrop(Gdiplus::Point(z.graph.left,z.graph.top),Gdiplus::Point(z.graph.left,z.graph.bottom),Color(255,26,20,36),Color(255,15,14,24));g.FillRectangle(&backdrop,graph);
        auto saved=g.Save();g.SetClip(graph);
        for(double hz:{20.,30.,40.,50.,70.,100.,150.,200.,300.,400.,500.,700.,1000.,1500.,2000.,3000.,4000.,5000.,7000.,10000.,15000.,20000.}){Pen grid(Color(255,43,34,52),1);g.DrawLine(&grid,z.x(hz),z.graph.top,z.x(hz),z.graph.bottom);}
        for(int db=-18;db<=18;db+=3){Pen grid(db==0?Color(255,115,82,109):Color(255,47,37,55),db==0?1.5f:1.f);g.DrawLine(&grid,z.graph.left,z.y(db),z.graph.right,z.y(db));}
        if(fx.analyzerEnabled&&fx.eqAnalysis){
            auto head=fx.eqAnalysis->head.load(std::memory_order_acquire);
            if(head!=fx.eqSpectrumHead){
                std::array<std::complex<double>,4096>signal,signalR;
                auto&ring=fx.analyzerPre?fx.eqAnalysis->input:fx.eqAnalysis->output;
                auto&right=fx.analyzerPre?fx.eqAnalysis->inputR:fx.eqAnalysis->outputR;
                for(size_t n=0;n<signal.size();n++){double window=.5-.5*std::cos(6.283185307179586*n/4095);signal[n]=ring[(head+n)%4096].load(std::memory_order_relaxed)*window;signalR[n]=right[(head+n)%4096].load(std::memory_order_relaxed)*window;}
                fftFx(signal);fftFx(signalR);
                for(int i=0;i<512;i++){
                    double hz=20*std::pow(1000.,i/511.);double bin=clamp(hz/fx.rate*4096,1,2047);int a=(int)bin,b=std::min(2047,a+1);double fraction=bin-a;
                    double magnitude=(std::sqrt((std::norm(signal[a])+std::norm(signalR[a]))*.5)*(1-fraction)+std::sqrt((std::norm(signal[b])+std::norm(signalR[b]))*.5)*fraction)/1024.;
                    float value=(float)clamp(20*std::log10(std::max(1e-8,magnitude)),-96,0);
                    fx.eqSpectrum[i]=fx.eqSpectrumHead==~uint64_t(0)?value:std::max(value,fx.eqSpectrum[i]*.7f+value*.3f);
                }fx.eqSpectrumHead=head;
            }
            GraphicsPath shape;shape.StartFigure();shape.AddLine((int)z.graph.left,(int)z.graph.bottom,(int)z.graph.left,(int)z.graph.bottom);
            for(int i=0;i<512;i++){float x=z.graph.left+i/511.f*(z.graph.right-z.graph.left),y=z.graph.bottom-(fx.eqSpectrum[i]+96)/96*(z.graph.bottom-z.graph.top);auto previous=i?PointF(z.graph.left+(i-1)/511.f*(z.graph.right-z.graph.left),z.graph.bottom-(fx.eqSpectrum[i-1]+96)/96*(z.graph.bottom-z.graph.top)):PointF((float)z.graph.left,(float)z.graph.bottom);shape.AddLine(previous,PointF(x,y));}
            shape.AddLine((int)z.graph.right,(int)z.graph.bottom,(int)z.graph.left,(int)z.graph.bottom);shape.CloseFigure();
            LinearGradientBrush spectrum(Gdiplus::Point(z.graph.left,z.graph.top),Gdiplus::Point(z.graph.left,z.graph.bottom),Color(110,219,67,111),Color(8,148,74,166));g.FillPath(&spectrum,&shape);
        }
        std::array<NativeBiquad,7>filters;for(int band=0;band<7;band++){int base=eqBandBase(band);filters[band].set(eqFilterType(eqMode(fx,band)),eqValue(fx,base),eqValue(fx,base+2),eqValue(fx,base+1),fx.rate);}
        int points=std::max(2,std::min(800,(int)(z.graph.right-z.graph.left)));std::vector<PointF>total(points),selected(points);
        double wet=eqValue(fx,18)*.01,output=std::pow(10.,eqValue(fx,19)/20.);
        for(int i=0;i<points;i++){
            int x=z.graph.left+(int)(i/double(points-1)*(z.graph.right-z.graph.left));double hz=z.frequency(x);std::complex<double>response=1.;for(auto&filter:filters)response*=filter.response(hz,fx.rate);
            total[i]=PointF((float)x,(float)z.y(20*std::log10(std::max(1e-12,std::abs(((1-wet)+wet*response)*output)))));
            selected[i]=PointF((float)x,(float)z.y(20*std::log10(std::max(1e-12,std::abs(filters[fx.eqSelected].response(hz,fx.rate))))));
        }
        COLORREF selectedColor=eqColors[fx.eqSelected];auto color=[&](int a,COLORREF c){return Color((BYTE)a,GetRValue(c),GetGValue(c),GetBValue(c));};
        GraphicsPath bandFill;bandFill.AddLines(selected.data(),points);bandFill.AddLine(selected.back(),PointF((float)z.graph.right,(float)z.y(0)));bandFill.AddLine(PointF((float)z.graph.right,(float)z.y(0)),PointF((float)z.graph.left,(float)z.y(0)));bandFill.CloseFigure();SolidBrush wash(color(22,selectedColor));g.FillPath(&wash,&bandFill);
        Pen bandPen(color(150,selectedColor),1.2f),glow(Color(35,235,100,141),7),curve(Color(255,246,220,233),2);g.DrawLines(&bandPen,selected.data(),points);g.DrawLines(&glow,total.data(),points);g.DrawLines(&curve,total.data(),points);
        for(int band=0;band<7;band++){
            int base=eqBandBase(band),mode=eqMode(fx,band);float x=(float)z.x(eqValue(fx,base)),y=(float)z.y(mode>=3?0:eqValue(fx,base+1));COLORREF c=eqColors[band];
            if(band==fx.eqSelected){Pen ring(color(115,c),2);g.DrawEllipse(&ring,x-15,y-15,30.f,30.f);Pen guide(color(85,c),1);guide.SetDashStyle(DashStyleDot);g.DrawLine(&guide,x,(float)z.graph.top,x,(float)z.graph.bottom);}
            SolidBrush point(mode==7?Color(255,59,50,66):color(255,c));g.FillEllipse(&point,x-10,y-10,20.f,20.f);
        }g.Restore(saved);
    }
    for(int db=-18;db<=18;db+=6)textFx(dc,(db>0?L"+":L"")+std::to_wstring(db),{3,z.y(db)-10,46,z.y(db)+10},mutedInk,DT_RIGHT|DT_SINGLELINE|DT_VCENTER);
    for(int db=-12;db>=-84;db-=24){int y=z.graph.bottom-(db+96)*(z.graph.bottom-z.graph.top)/96;textFx(dc,std::to_wstring(db),{z.graph.right+6,y-9,width-3,y+9},mutedInk);}
    int previous=-100;for(int hz:{20,50,100,200,500,1000,2000,5000,10000,20000}){int x=z.x(hz);if(x-previous<45&&hz!=20000)continue;previous=x;textFx(dc,hz>=1000?std::to_wstring(hz/1000)+L"k":std::to_wstring(hz),{x-24,z.graph.bottom+3,x+26,z.graph.bottom+23},mutedInk,DT_CENTER|DT_SINGLELINE|DT_VCENTER);}
    for(int band=0;band<7;band++){
        int base=eqBandBase(band),mode=eqMode(fx,band),x=z.x(eqValue(fx,base)),y=z.y(mode>=3?0:eqValue(fx,base+1));textFx(dc,std::to_wstring(band+1),{x-9,y-10,x+10,y+10},RGB(24,16,27),DT_CENTER|DT_SINGLELINE|DT_VCENTER);
        auto r=z.tab(band);fillFx(dc,r,band==fx.eqSelected?RGB(57,34,53):panelColor);textFx(dc,std::to_wstring(band+1)+L"  "+(mode==7?L"OFF":numberFx(eqValue(fx,base+1),1)+L" dB"),r,mode==7?mutedInk:eqColors[band],DT_CENTER|DT_SINGLELINE|DT_VCENTER);
    }
    int base=eqBandBase(fx.eqSelected);const wchar_t*labels[]={L"Frequency",L"Gain",L"Bandwidth / Q",L"Filter type ▾"};
    for(int i=0;i<4;i++){auto r=z.detail(i);fillFx(dc,r,panelColor);textFx(dc,labels[i],{r.left+10,r.top+1,r.right-10,r.top+23},mutedInk);std::wstring value=i==3?eqTypes[eqMode(fx,fx.eqSelected)]:numberFx(eqValue(fx,base+i),i==0?1:2)+(i==0?L" Hz":i==1?L" dB":L"");textFx(dc,value,{r.left+10,r.top+22,r.right-10,r.bottom-1},eqColors[fx.eqSelected]);}
    for(int i=0;i<2;i++){auto r=z.output(i);fillFx(dc,r,panelColor);textFx(dc,i==0?L"Mix  "+numberFx(eqValue(fx,18),1)+L" %":L"Output  "+numberFx(eqValue(fx,19),1)+L" dB",r,ink,DT_CENTER|DT_SINGLELINE|DT_VCENTER);}
}
void eqBandMenu(NativeEffect&fx,HWND window){
    HMENU menu=CreatePopupMenu();int mode=eqMode(fx,fx.eqSelected);
    for(int i=0;i<8;i++)AppendMenuW(menu,MF_STRING|(mode==i?MF_CHECKED:0),100+i,ui::translate(eqTypes[i]).c_str());
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);const wchar_t*exact[]={L"Frequency…",L"Gain…",L"Bandwidth / Q…",L"Reset band"};for(int i=0;i<4;i++)AppendMenuW(menu,MF_STRING,200+i,ui::translate(exact[i]).c_str());
    POINT point;GetCursorPos(&point);int command=TrackPopupMenu(menu,TPM_RETURNCMD,point.x,point.y,0,window,nullptr);DestroyMenu(menu);
    if(command>=100&&command<108)fx.set(23+fx.eqSelected,fx.spec().parameters[23+fx.eqSelected].normalized(command-100),true);
    int base=eqBandBase(fx.eqSelected);if(command==203){for(int i=0;i<3;i++){auto&p=fx.spec().parameters[base+i];fx.set(base+i,p.normalized(p.initial),true);}fx.set(23+fx.eqSelected,0,true);}
    if(command>=200&&command<203){int id=base+command-200;auto&p=fx.spec().parameters[id];auto value=TextPrompt::show(window,ui::translate(wide(p.name))+L" / "+wide(p.unit),numberFx(eqValue(fx,id),4));if(value)try{size_t end=0;double n=std::stod(*value,&end);if(end!=value->size()||!std::isfinite(n))throw std::runtime_error("Invalid number");fx.set(id,p.normalized(n),true);}catch(...){MessageBoxW(window,ui::translate(L"Enter a valid number").c_str(),L"Blood EQ",MB_OK);}}
}
bool equalizerInput(NativeEffect&fx,HWND window,UINT message,WPARAM w,LPARAM l){
    RECT client;GetClientRect(window,&client);EqLayout z(client.right,client.bottom);int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
    if(message==WM_SHOWWINDOW){fx.analyzerRunning=w&&fx.analyzerEnabled;return false;}
    if(message==WM_CLOSE||message==WM_DESTROY){fx.analyzerRunning=false;fx.eqDragging=false;return false;}
    if(message==WM_CAPTURECHANGED){fx.eqDragging=false;return false;}
    if(message==WM_LBUTTONUP&&fx.eqDragging){fx.eqDragging=false;ReleaseCapture();return true;}
    if(message==WM_MOUSEMOVE&&fx.eqDragging&&(w&MK_LBUTTON)){
        double fine=GetKeyState(VK_CONTROL)&0x8000?.15:1.;int base=eqBandBase(fx.eqSelected);
        fx.set(base,fx.dragValue+(x-fx.dragX)/double(z.graph.right-z.graph.left)*fine,true);
        if(eqMode(fx,fx.eqSelected)<3)fx.set(base+1,fx.eqDragGain-(y-fx.eqDragY)/double(z.graph.bottom-z.graph.top)*fine,true);return true;
    }
    if(message==WM_LBUTTONDOWN){
        if(y>=126&&y<149&&x>=z.w-300){if(x<z.w-164){fx.analyzerEnabled=!fx.analyzerEnabled;fx.analyzerRunning=fx.analyzerEnabled;}else if(x>=z.w-156){fx.analyzerPre=!fx.analyzerPre;fx.eqSpectrumHead=~uint64_t(0);}return true;}
        for(int band=0;band<7;band++)if(inEqRect(z.tab(band),x,y)){fx.eqSelected=band;SetFocus(window);return true;}
        if(inEqRect(z.detail(3),x,y)){eqBandMenu(fx,window);return true;}
        if(inEqRect(z.graph,x,y)){fx.eqSelected=eqPointBand(fx,z,x,y);fx.eqDragging=true;fx.dragX=(float)x;fx.eqDragY=(float)y;int base=eqBandBase(fx.eqSelected);fx.dragValue=fx.values[base].load();fx.eqDragGain=fx.values[base+1].load();SetFocus(window);SetCapture(window);return true;}
    }
    if(message==WM_LBUTTONDBLCLK&&inEqRect(z.graph,x,y)){int band=eqPointBand(fx,z,x,y);fx.eqSelected=band;fx.set(eqBandBase(band)+1,fx.spec().parameters[eqBandBase(band)+1].normalized(0),true);return true;}
    if(message==WM_RBUTTONUP&&inEqRect(z.graph,x,y)){fx.eqSelected=eqPointBand(fx,z,x,y);eqBandMenu(fx,window);return true;}
    if(message==WM_MOUSEWHEEL){
        POINT point{x,y};ScreenToClient(window,&point);if(inEqRect(z.graph,point.x,point.y)){int nearbyBand=eqPointBand(fx,z,point.x,point.y,true);if(nearbyBand>=0)fx.eqSelected=nearbyBand;int id=eqBandBase(fx.eqSelected)+2;fx.set(id,fx.values[id].load()+GET_WHEEL_DELTA_WPARAM(w)/120.*((GET_KEYSTATE_WPARAM(w)&MK_CONTROL)?.005:.035),true);return true;}
    }
    if(message==WM_KEYDOWN){int base=eqBandBase(fx.eqSelected);bool control=GetKeyState(VK_CONTROL)&0x8000;
        if(w==VK_LEFT||w==VK_RIGHT){fx.set(base,fx.values[base].load()+(w==VK_LEFT?-1:1)*(control?.001:.005),true);return true;}
        if(w==VK_UP||w==VK_DOWN){fx.set(base+1,fx.spec().parameters[base+1].normalized(eqValue(fx,base+1)+(w==VK_DOWN?-1:1)*(control?.1:.5)),true);return true;}
        if(w==VK_TAB){fx.eqSelected=(fx.eqSelected+((GetKeyState(VK_SHIFT)&0x8000)?6:1))%7;return true;}
        if(w==VK_ESCAPE){SendMessageW(window,WM_CLOSE,0,0);return true;}
    }
    return false;
}
}
