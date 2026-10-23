    struct StemWidget {int id;Rect rect;};
    std::vector<StemWidget>stemWidgets;int stemSettingsTab=0;float stemPanelHeight=480,stemFormY=0,stemRowStep=56,stemChecksY=0,stemPreviewY=0,stemActionY=0;HWND stemTooltip=nullptr,stemHover=nullptr;std::wstring stemTipText;
    std::array<int,6>stemPageFields()const{return stemSettingsTab?std::array<int,6>{713,714,715,716,717,721}:std::array<int,6>{710,711,712,718,719,720};}
    const wchar_t*stemFieldLabel(int id)const{switch(id){case 710:return L"Stem model";case 711:return L"Extract";case 712:return L"Quality";case 713:return L"Shift passes";case 714:return L"Overlap";case 715:return L"Segment / memory";case 716:return L"CPU threads";case 717:return L"Device";case 718:return L"File format";case 719:return L"Sample rate";case 720:return L"Levels";case 721:return L"Gain";}return L"";}
    void layoutStemControls(){
        if(!stemWindow)return;RECT client{};GetClientRect(stemWindow,&client);stemZoom=clamp(scale,.85,2.5);stemWidth=std::max(1.,client.right/stemZoom);stemPanelHeight=std::max(1.,client.bottom/stemZoom);
        const float m=12,gap=8,w=stemWidth-2*m;bool compact=stemPanelHeight<440;float control=compact?26:32,header=compact?32:46,tabHeight=compact?26:30;
        stemColumns=stemWidth<620&&stemPanelHeight>=460?2:3;float sourceY=header+4,tabY=sourceY+control+8;stemFormY=tabY+tabHeight+10;
        stemActionY=stemPanelHeight-m-control;stemPreviewY=stemActionY-control-8;stemStatusY=stemPreviewY-31;stemFolderY=stemStatusY-control-6;stemChecksY=stemFolderY-(compact?26:32);
        int rows=(6+stemColumns-1)/stemColumns;stemRowStep=std::min(70.f,(stemChecksY-stemFormY-5)/rows);float label=std::min(19.f,stemRowStep*.34f),fieldHeight=std::min(control,stemRowStep-label-5),cell=(w-(stemColumns-1)*gap)/stemColumns;
        stemWidgets.clear();auto add=[&](int id,Rect rect){stemWidgets.push_back({id,rect});};
        add(742,{stemWidth-m-30,2,30,compact?26.f:30.f});
        float browse=compact?72:90,use=compact?94:110;
        add(700,{m,sourceY,w-browse-use-2*gap,control});add(701,{stemWidth-m-use-gap-browse,sourceY,browse,control});add(702,{stemWidth-m-use,sourceY,use,control});
        float tabs=std::min(200.f,(w-gap)/2);add(740,{m,tabY,tabs,tabHeight});add(741,{m+tabs+gap,tabY,tabs,tabHeight});
        auto fields=stemPageFields();for(int i=0;i<6;i++)add(fields[i],{m+(i%stemColumns)*(cell+gap),stemFormY+(i/stemColumns)*stemRowStep+label,cell,fieldHeight});
        float checkCell=(w-2*gap)/3;for(int i=0;i<3;i++)add((stemSettingsTab?722:725)+i,{m+i*(checkCell+gap),stemChecksY,checkCell,compact?22.f:26.f});
        add(731,{m,stemFolderY,w-2*control-2*gap,control});add(730,{stemWidth-m-2*control-gap,stemFolderY,control,control});add(734,{stemWidth-m-control,stemFolderY,control,control});
        add(736,{m,stemPreviewY-7,w,3});add(737,{m,stemPreviewY,w-2*control-2*gap,control});add(738,{stemWidth-m-2*control-gap,stemPreviewY,control,control});add(739,{stemWidth-m-control,stemPreviewY,control,control});
        float cancel=compact?70:86,primary=w*.43f;add(732,{m,stemActionY,primary,control});add(733,{m+primary+gap,stemActionY,cancel,control});add(735,{m+primary+cancel+2*gap,stemActionY,w-primary-cancel-2*gap,control});
        HDWP batch=BeginDeferWindowPos((int)stemControls.size());for(auto c:stemControls){HWND child=GetDlgItem(stemWindow,c.id);auto found=std::find_if(stemWidgets.begin(),stemWidgets.end(),[&](const auto&v){return v.id==c.id;});
            if(found==stemWidgets.end()){ShowWindow(child,SW_HIDE);continue;}auto r=found->rect;UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW|SWP_NOCOPYBITS;int height=(int)std::round(r.h*stemZoom);if(c.id>=710&&c.id<=721||c.id==737)height+=(int)(240*stemZoom);
            if(batch)batch=DeferWindowPos(batch,child,nullptr,(int)std::round(r.x*stemZoom),(int)std::round(r.y*stemZoom),(int)std::round(r.w*stemZoom),height,flags);
            else SetWindowPos(child,nullptr,(int)std::round(r.x*stemZoom),(int)std::round(r.y*stemZoom),(int)std::round(r.w*stemZoom),height,flags);
            if(c.id>=710&&c.id<=721||c.id==737){SendMessageW(child,CB_SETITEMHEIGHT,(WPARAM)-1,std::max(12,(int)std::round(r.h*stemZoom)-6));SendMessageW(child,CB_SETDROPPEDWIDTH,(WPARAM)std::min(420.,std::max(230.,double(r.w))*stemZoom),0);}
        }if(batch)EndDeferWindowPos(batch);RedrawWindow(stemWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
    }
    std::wstring stemControlText(HWND h){int length=GetWindowTextLengthW(h);std::wstring s(length+1,L'\0');GetWindowTextW(h,s.data(),length+1);s.resize(length);return s;}
    void drawStemWidget(Painter&p,int id,float w,float h,HWND child){
        bool enabled=IsWindowEnabled(child),hover=child==stemHover,focus=GetFocus()==child;Color ink=enabled?textColor:muted;Rect r{0,0,w,h};bool combo=(id>=710&&id<=721)||id==737,check=id>=722&&id<=727,tab=id==740||id==741;
        if(id==736){p.fill(r,lineColor,2);p.fill({0,0,w*(float)clamp(stems.progress.load(),0,1),h},crimson,2);return;}
        if(tab){bool active=id==740+stemSettingsTab;p.fill(r,active?alpha(crimson,28):raised,6);if(active)p.fill({8,h-2,w-16,2},crimson,1);p.text(stemControlText(child),r,11.5,active?crimson:ink,active,1);return;}
        if(check){bool checked=stemCheck(id);p.fill(r,panel);p.fill({0,(h-15)/2,15,15},checked?crimson:lineColor,4);if(checked)p.text(L"✓",{0,(h-17)/2,15,17},12,bg,true,1);p.text(stemControlText(child),{22,0,w-22,h},11,ink);if(focus)p.line(22,h-1,w,h-1,crimson);return;}
        bool primary=id==732,readOnly=id==700||id==731;
        p.fill(r,focus?crimson:hover&&enabled?alpha(crimson,135):lineColor,6);p.fill({1,1,w-2,h-2},primary&&enabled?crimson:hover&&enabled?alpha(crimson,35):raised,5);
        std::wstring text=stemControlText(child);if(id==731){auto path=fs::path(text);text=L"↳ "+path.wstring();}if(id==737&&text.empty())text=ui::translate(L"Preview a stem after separation");
        if(combo){p.text(text,{9,0,w-31,h},11.5,ink,false,0,false);float x=w-13,y=h/2;p.line(x-3,y-2,x,y+1,ink,1.3f);p.line(x,y+1,x+3,y-2,ink,1.3f);}
        else p.text(text,{readOnly?9.f:4.f,0,w-(readOnly?18.f:8.f),h},id==700||id==731?10.5f:11.5f,primary&&enabled?bg:ink,primary,readOnly?0:1,false);
    }
    void paintStemControl(HWND child,HDC dc,int id){RECT r{};GetClientRect(child,&r);Graphics g(dc);g.ScaleTransform((float)stemZoom,(float)stemZoom);Painter p(g,fonts);drawStemWidget(p,id,(float)(r.right/stemZoom),(float)(r.bottom/stemZoom),child);}
    void paintStemBackground(Painter&p){
        p.fill({0,0,stemWidth,stemPanelHeight},panel);bool compact=stemPanelHeight<440;p.bat(28,compact?17:23,.42f,crimson);p.text(L"BLOOD STEMS",{48,compact?2.f:5.f,stemWidth-100,25},compact?16:19,textColor,true);
        if(!compact)p.text(L"Your sound, separated. Entirely on your PC.",{49,29,stemWidth-70,15},9.5,muted);
        auto fields=stemPageFields();float cell=(stemWidth-24-(stemColumns-1)*8)/stemColumns,label=std::min(19.f,stemRowStep*.34f);for(int i=0;i<6;i++)p.text(stemFieldLabel(fields[i]),{12+(i%stemColumns)*(cell+8),stemFormY+(i/stemColumns)*stemRowStep,cell,label},10,muted);
        auto text=stemStatusText(stems.running,stemResult,stems.status());if(!stems.running&&stemFlag(stemResult,"passed")){text=ui::translate(L"Complete")+L" · "+number(stems.samples.size())+L" "+ui::translate(L"stems")+L" · "+wide(stemText(stemResult,"device","cpu"));if(stemFlag(stemResult,"device_fallback"))text+=L" · "+ui::translate(L"CUDA unavailable; using CPU");}
        p.circle(16,stemStatusY+10,3,stems.running?crimson:muted);p.text(text,{27,stemStatusY,stemWidth-39,21},10,stems.running?crimson:muted);
    }
    static LRESULT CALLBACK stemControlProc(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data){auto*a=(App*)data;
        try{if(msg==WM_ERASEBKGND)return 1;if(msg==WM_PAINT){WindowPaintBuffer buffer(h);a->paintStemControl(h,buffer.dc,(int)id);return 0;}
            if(msg==WM_MOUSEWHEEL&&!SendMessageW(h,CB_GETDROPPEDSTATE,0,0))return 0;
            if(msg==WM_MOUSEMOVE&&a->stemHover!=h){auto old=a->stemHover;a->stemHover=h;if(old)InvalidateRect(old,nullptr,FALSE);InvalidateRect(h,nullptr,FALSE);TRACKMOUSEEVENT tracking{sizeof tracking,TME_LEAVE,h,0};TrackMouseEvent(&tracking);}
            if(msg==WM_MOUSELEAVE){a->stemHover=nullptr;InvalidateRect(h,nullptr,FALSE);}if(msg==WM_SETFOCUS||msg==WM_KILLFOCUS||msg==WM_ENABLE||msg==BM_SETCHECK||msg==CB_SETCURSEL)InvalidateRect(h,nullptr,FALSE);
            if(msg==WM_NCDESTROY)RemoveWindowSubclass(h,stemControlProc,id);
        }catch(const std::exception&e){a->queueStemError(e.what(),true);return 0;}catch(...){a->queueStemError("Blood Stems control error",true);return 0;}
        return DefSubclassProc(h,msg,w,l);
    }
    void saveStemPreview(const fs::path&file){
        RECT client{};GetClientRect(stemWindow,&client);Bitmap bitmap(client.right,client.bottom,PixelFormat32bppPARGB);{Graphics g(&bitmap);g.ScaleTransform((float)stemZoom,(float)stemZoom);Painter p(g,fonts);paintStemBackground(p);
            for(auto widget:stemWidgets){auto state=g.Save();g.TranslateTransform(widget.rect.x,widget.rect.y);drawStemWidget(p,widget.id,widget.rect.w,widget.rect.h,GetDlgItem(stemWindow,widget.id));g.Restore(state);}}
        UINT size=0,count=0;GetImageEncodersSize(&count,&size);std::vector<BYTE>data(size);auto codecs=(ImageCodecInfo*)data.data();GetImageEncoders(count,size,codecs);for(UINT i=0;i<count;i++)if(!wcscmp(codecs[i].MimeType,L"image/png")){fs::create_directories(file.parent_path());if(bitmap.Save(file.c_str(),&codecs[i].Clsid,nullptr)!=Ok)throw std::runtime_error("Cannot save Blood Stems preview");return;}throw std::runtime_error("PNG encoder unavailable");
    }
