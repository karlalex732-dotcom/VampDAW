namespace {
int assistantScenario(NativeEffect&fx){return(int)fx.spec().parameters[0].value(fx.values[0].load());}
double assistantStrength(NativeEffect&fx){return fx.spec().parameters[1].value(fx.values[1].load());}
int assistantSeconds(NativeEffect&fx){return(int)fx.spec().parameters[2].value(fx.values[2].load());}
void assistantPreview(NativeEffect&fx){auto previous=fx.assistantPlan;fx.assistantPlan=makeAssistantPlan(assistantScenario(fx),assistantStrength(fx),previous.analyzed?&previous.analysis:nullptr);fx.assistantApplied=false;}
void assistantResize(NativeEffect&fx){RECT r;GetClientRect(fx.window,&r);if(fx.assistantCombo)MoveWindow(fx.assistantCombo,22,187,std::max(100,(int)r.right-44),420,TRUE);if(fx.assistantSlider)MoveWindow(fx.assistantSlider,215,123,std::max(100,(int)r.right-237),32,TRUE);InvalidateRect(fx.window,nullptr,FALSE);}
RECT assistantButton(int width,int index){int gap=8,cell=(width-44-gap*3)/4;return{22+index*(cell+gap),230,22+index*(cell+gap)+cell,266};}
void paintAssistant(NativeEffect&fx){
    PAINTSTRUCT ps;HDC target=BeginPaint(fx.window,&ps);RECT r;GetClientRect(fx.window,&r);int w=r.right,h=r.bottom;
    HDC dc=CreateCompatibleDC(target);auto bmp=CreateCompatibleBitmap(target,std::max(1,w),std::max(1,h));auto oldBmp=SelectObject(dc,bmp);
    fillFx(dc,{0,0,w,h},background);fillFx(dc,{0,0,w,4},accent);
    auto font=CreateFontW(16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");auto title=CreateFontW(28,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");auto oldFont=SelectObject(dc,title);
    textFx(dc,L"ORACLE ASSIST",{22,15,w-22,56});SelectObject(dc,font);
    textFx(dc,L"Local sound analysis · 30 scenarios · Vamp effect chains",{23,58,w-22,84},mutedInk);
    textFx(dc,L"Analyzes the signal at this slot. Set the listening time below.",{22,92,w-22,120});
    textFx(dc,L"Listen duration: "+std::to_wstring(assistantSeconds(fx))+ui::translate(L" s"),{22,124,213,154},accent);
    textFx(dc,L"Scenario",{22,160,w-22,184},mutedInk);
    const auto state=fx.assistantCapture?fx.assistantCapture->state.load(std::memory_order_acquire):0;
    const std::wstring labels[]={state==1||state==2?L"Listening…":L"Analyze sound",fx.assistantAuto?L"Auto profile: ON":L"Auto profile: OFF",L"Amount "+numberFx(assistantStrength(fx),0)+L"%",fx.assistantApplied?L"Applied":L"Apply chain"};
    for(int i=0;i<4;i++){auto box=assistantButton(w,i);fillFx(dc,box,i==3?RGB(77,29,49):panelColor);textFx(dc,labels[i],box,i==1&&fx.assistantAuto?accent:ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    RECT progress{22,276,w-22,282};fillFx(dc,progress,panelColor);if(state==1||state==2){progress.right=22+(int)((w-44)*fx.assistantCapture->progress.load()/double(fx.assistantCapture->targetFrames.load()));fillFx(dc,progress,accent);}
    std::wstring info=fx.assistantStatus.empty()?L"Choose a scenario or enable Auto profile, then Analyze sound.":fx.assistantStatus;
    textFx(dc,info,{22,289,w-22,318},accent);
    if(fx.assistantPlan.analyzed){auto&a=fx.assistantPlan.analysis;textFx(dc,L"Peak "+numberFx(a.peak)+L" dBFS · RMS "+numberFx(a.rms)+L" dBFS · Crest "+numberFx(a.crest)+L" dB · Correlation "+numberFx(a.correlation,2),{22,320,w-22,347},mutedInk);}
    else textFx(dc,L"Preset preview / no sound analysis yet",{22,320,w-22,347},mutedInk);
    int y=361-fx.scroll,saved=SaveDC(dc);IntersectClipRect(dc,0,356,w,h-62);
    for(size_t i=0;i<fx.assistantPlan.steps.size();i++){auto&step=fx.assistantPlan.steps[i];if(y+72>356&&y<h-62){fillFx(dc,{22,y,w-22,y+70},panelColor);textFx(dc,std::to_wstring(i+1)+L"  "+wide(nativeSpecs()[step.kind].name),{34,y+1,w-34,y+24});textFx(dc,wide(step.reason),{34,y+23,w-34,y+44},mutedInk);std::wstring settings;for(auto&item:step.settings){auto&p=nativeSpecs()[step.kind].parameters[item.first];if(!settings.empty())settings+=L" · ";settings+=wide(p.name)+L" "+numberFx(item.second,p.stepped?0:1)+L" "+wide(p.unit);}textFx(dc,settings,{34,y+44,w-34,y+68},mutedInk);}y+=76;}
    RestoreDC(dc,saved);fillFx(dc,{0,h-60,w,h},background);textFx(dc,L"Appends effects to free slots. Existing effects remain; Undo restores the chain.",{22,h-58,w-22,h-32},mutedInk);
    textFx(dc,L"Local analyzer, not a neural network. Verify the result by listening.",{22,h-32,w-22,h-6},mutedInk);
    paintFxHover(fx,dc);BitBlt(target,0,0,w,h,dc,0,0,SRCCOPY);SelectObject(dc,oldFont);DeleteObject(title);DeleteObject(font);SelectObject(dc,oldBmp);DeleteObject(bmp);DeleteDC(dc);EndPaint(fx.window,&ps);
}
LRESULT assistantWindowProc(NativeEffect&fx,HWND h,UINT message,WPARAM w,LPARAM l){
    auto refresh=[&]{RECT r;GetClientRect(h,&r);fx.scroll=(int)clamp(fx.scroll,0,std::max(0,(int)fx.assistantPlan.steps.size()*76-((int)r.bottom-423)));SCROLLINFO si{sizeof si,SIF_RANGE|SIF_PAGE|SIF_POS};si.nMax=std::max(0,(int)fx.assistantPlan.steps.size()*76);si.nPage=std::max(1,(int)r.bottom-423);si.nPos=fx.scroll;SetScrollInfo(h,SB_VERT,&si,TRUE);int capture=fx.assistantCapture?fx.assistantCapture->state.load():0;if(fx.assistantSlider)EnableWindow(fx.assistantSlider,capture==0);InvalidateRect(h,nullptr,FALSE);};
    switch(message){
    case WM_CREATE:{INITCOMMONCONTROLSEX cc{sizeof cc,ICC_BAR_CLASSES};InitCommonControlsEx(&cc);fx.assistantSlider=CreateWindowExW(0,TRACKBAR_CLASSW,L"Listen duration",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_NOTICKS|TBS_TOOLTIPS,215,123,500,32,h,(HMENU)902,GetModuleHandleW(nullptr),nullptr);SendMessageW(fx.assistantSlider,TBM_SETRANGE,TRUE,MAKELPARAM(1,30));SendMessageW(fx.assistantSlider,TBM_SETPAGESIZE,0,5);SendMessageW(fx.assistantSlider,TBM_SETPOS,TRUE,assistantSeconds(fx));fx.assistantCombo=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,22,187,800,420,h,(HMENU)901,GetModuleHandleW(nullptr),nullptr);SendMessageW(fx.assistantCombo,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);for(auto&p:assistantPresets()){auto name=ui::translate(wide(p.name));SendMessageW(fx.assistantCombo,CB_ADDSTRING,0,(LPARAM)name.c_str());}SendMessageW(fx.assistantCombo,CB_SETCURSEL,assistantScenario(fx),0);assistantPreview(fx);assistantResize(fx);return 0;}
    case WM_PAINT:paintAssistant(fx);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:assistantResize(fx);refresh();return 0;
    case WM_GETMINMAXINFO:((MINMAXINFO*)l)->ptMinTrackSize={680,610};return 0;
    case WM_CLOSE:closePanelWindow(h);return 0;
    case WM_DESTROY:KillTimer(h,1);fx.assistantCombo=fx.assistantSlider=nullptr;fx.window=nullptr;return 0;
    case WM_TIMER:{if(!IsWindowVisible(h))return 0;SendMessageW(fx.assistantSlider,TBM_SETPOS,TRUE,assistantSeconds(fx));if(fx.assistantCapture&&fx.assistantCapture->state.load(std::memory_order_acquire)==3){auto&capture=*fx.assistantCapture;auto a=analyzeAssistant(capture.left.data(),capture.right.data(),capture.targetFrames.load(),capture.rate);capture.state.store(0,std::memory_order_release);if(a.valid){if(fx.assistantAuto){int preset=detectAssistantPreset(a,fx.assistantMaster);fx.set(0,fx.spec().parameters[0].normalized(preset),true);SendMessageW(fx.assistantCombo,CB_SETCURSEL,preset,0);}fx.assistantPlan=makeAssistantPlan(assistantScenario(fx),assistantStrength(fx),&a);fx.assistantApplied=false;fx.assistantStatus=L"Analysis ready · "+std::to_wstring(fx.assistantPlan.steps.size())+L" effects · review before applying";}else{fx.assistantPlan={};fx.assistantStatus=L"Signal too quiet. Raise the source level and analyze again.";}refresh();}else refresh();return 0;}
    case WM_HSCROLL:if((HWND)l==fx.assistantSlider){int capture=fx.assistantCapture?fx.assistantCapture->state.load():0;if(capture==0){fx.set(2,fx.spec().parameters[2].normalized(SendMessageW(fx.assistantSlider,TBM_GETPOS,0,0)),true);InvalidateRect(h,nullptr,FALSE);}}return 0;
    case WM_COMMAND:if(LOWORD(w)==901&&HIWORD(w)==CBN_SELCHANGE){int preset=(int)SendMessageW(fx.assistantCombo,CB_GETCURSEL,0,0);fx.set(0,fx.spec().parameters[0].normalized(preset),true);fx.assistantAuto=false;assistantPreview(fx);fx.assistantStatus=L"Scenario selected · review the chain, or analyze to adapt it";refresh();}return 0;
    case WM_RBUTTONUP:{int row=(GET_Y_LPARAM(l)-361+fx.scroll)/76;if(GET_Y_LPARAM(l)>=356&&row>=0&&row<(int)fx.assistantPlan.steps.size()){auto&step=fx.assistantPlan.steps[row];std::wstring details=ui::translate(wide(step.reason))+L"\n\n";for(auto&item:step.settings){auto&p=nativeSpecs()[step.kind].parameters[item.first];details+=ui::translate(wide(p.name))+L": "+numberFx(item.second,p.stepped?0:2)+L" "+wide(p.unit)+L"\n";}MessageBoxW(h,details.c_str(),wide(nativeSpecs()[step.kind].name).c_str(),MB_OK);}return 0;}
    case WM_LBUTTONDOWN:{RECT r;GetClientRect(h,&r);POINT pt{GET_X_LPARAM(l),GET_Y_LPARAM(l)};int button=-1;for(int i=0;i<4;i++){auto box=assistantButton(r.right,i);if(PtInRect(&box,pt))button=i;}
        int capture=fx.assistantCapture?fx.assistantCapture->state.load():0;
        if(button==0&&fx.assistantCapture&&fx.assistantCapture->request(assistantSeconds(fx))){fx.assistantStatus=L"Waiting for sound / listening: "+std::to_wstring(assistantSeconds(fx))+ui::translate(L" s");fx.assistantApplied=false;}
        if(button==1){fx.assistantAuto=!fx.assistantAuto;fx.edited=true;fx.assistantStatus=L"Auto profile uses technical features; select a scenario for a specific source.";}
        if(button==2){auto input=TextPrompt::show(h,ui::translate(L"Processing amount / 0–100%"),numberFx(assistantStrength(fx),0));if(input)try{auto text=*input;std::replace(text.begin(),text.end(),L',',L'.');size_t end=0;double value=std::stod(text,&end);if(end==text.size()&&std::isfinite(value)){fx.set(1,fx.spec().parameters[1].normalized(value),true);assistantPreview(fx);}}catch(...){} }
        if(button==3&&capture==0&&!fx.assistantApplied&&!fx.assistantPlan.steps.empty()){if(fx.assistantApply&&fx.assistantApply(fx.assistantPlan)){fx.assistantApplied=true;fx.assistantStatus=L"Chain added. Use Undo to restore the previous rack.";}else fx.assistantStatus=L"Cannot apply: check free slots, frozen track and recording state.";}
        refresh();return 0;}
    case WM_MOUSEWHEEL:fx.scroll-=GET_WHEEL_DELTA_WPARAM(w)/120*54;refresh();return 0;
    case WM_VSCROLL:{SCROLLINFO si{sizeof si,SIF_ALL};GetScrollInfo(h,SB_VERT,&si);int code=LOWORD(w);if(code==SB_THUMBTRACK)fx.scroll=si.nTrackPos;else fx.scroll+=code==SB_LINEUP?-54:code==SB_LINEDOWN?54:code==SB_PAGEUP?-(int)si.nPage:code==SB_PAGEDOWN?(int)si.nPage:0;refresh();return 0;}
    case WM_KEYDOWN:if(w==VK_ESCAPE){SendMessageW(h,WM_CLOSE,0,0);return 0;}if(w==VK_UP||w==VK_DOWN||w==VK_PRIOR||w==VK_NEXT){fx.scroll+=w==VK_UP?-54:w==VK_DOWN?54:w==VK_PRIOR?-216:216;refresh();return 0;}break;
    }return DefWindowProcW(h,message,w,l);
}
}
