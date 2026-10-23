// Regression checks run inside the application, using only its own hidden windows.
if(argc>2&&std::wstring(argv[1])==L"--stem-ui-check"){
    auto reportPath=fs::absolute(argv[2]);Json report={{"passed",false},{"version","0.4.10-alpha"},{"desktop_visual_inspection",false}};
    auto app=std::make_unique<App>();
    try{
        auto require=[](bool ok,const std::string&message){if(!ok)throw std::runtime_error(message);};
        app->root=reportPath.parent_path()/L"stem-ui-data";app->stateRoot=app->root/L"settings";fs::create_directories(app->stateRoot);
        GdiplusStartupInput input;require(GdiplusStartup(&app->gdipToken,&input,nullptr)==Ok,"GDI+ initialization");
        app->window=CreateWindowExW(0,L"STATIC",L"VampDAW internal regression test",WS_OVERLAPPED,0,0,1200,900,nullptr,nullptr,instance,nullptr);
        require(app->window!=nullptr,"Test parent creation");
        auto before=projectJson(app->project,app->root/L"unchanged.vamp");int paints=0,panels=0,layouts=0,wheels=0;Json previews=Json::array();
        for(const auto&prefs:{Json(),Json::object(),Json({{"710","invalid"},{"711",UINT64_MAX},{"724",17},{"outputRoot",false}})}){
            app->stemPreferences=prefs;app->openStems();require(app->stemWindow&&app->stemPendingError.empty(),"Open with missing or malformed preferences");panels++;
            for(double scale:{.85,1.,1.5,2.})for(auto size:std::array<POINT,4>{{{380,340},{500,440},{720,520},{1100,640}}})for(int tab=0;tab<2;tab++){
                app->scale=scale;app->stemSettingsTab=tab;SetWindowPos(app->stemWindow,nullptr,0,0,(int)std::round(size.x*scale),(int)std::round(size.y*scale),SWP_NOZORDER|SWP_NOACTIVATE);app->layoutStemControls();layouts++;
                require(!(GetWindowLongPtrW(app->stemWindow,GWL_STYLE)&(WS_VSCROLL|WS_HSCROLL)),"Panel has no scroll bars");
                require(app->stemWidgets.size()==25,"Every page contains its complete controls");
                RECT client{};GetClientRect(app->stemWindow,&client);std::vector<RECT>rects;
                for(auto widget:app->stemWidgets){auto r=widget.rect;auto child=GetDlgItem(app->stemWindow,widget.id);std::string context="control "+std::to_string(widget.id)+" at "+std::to_string(size.x)+"x"+std::to_string(size.y)+" / "+std::to_string(scale);
                    require(r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x+r.w<=app->stemWidth+1&&r.y+r.h<=app->stemPanelHeight+1,"Logical bounds: "+context);
                    require((GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE)!=0,"Visible: "+context);RECT actual{};GetWindowRect(child,&actual);MapWindowPoints(nullptr,app->stemWindow,(POINT*)&actual,2);
                    require(actual.left>=0&&actual.top>=0&&actual.right<=client.right&&actual.bottom<=client.bottom,"Native bounds: "+context);
                    for(auto previous:rects){RECT overlap{};require(!IntersectRect(&overlap,&actual,&previous),"Native overlap: "+context);}rects.push_back(actual);
                    if(widget.id>=710&&widget.id<=721){int choice=app->stemChoice(widget.id);SendMessageW(child,WM_MOUSEWHEEL,MAKEWPARAM(0,120),0);require(app->stemChoice(widget.id)==choice,"Closed combo wheel must not change settings");wheels++;}
                }
                auto anchor=app->stemWidgets.front().rect;SendMessageW(app->stemWindow,WM_MOUSEWHEEL,MAKEWPARAM(0,120),0);SendMessageW(app->stemWindow,WM_VSCROLL,SB_PAGEDOWN,0);require(app->stemWidgets.front().rect.y==anchor.y,"Wheel does not move panel");
                for(int language=0;language<2;language++){ui::language=language;for(const auto&result:{Json(),Json::object(),Json({{"passed",false}}),Json({{"passed",true}}),Json({{"passed","bad"}})}){
                    app->stemResult=result;InvalidateRect(app->stemWindow,nullptr,FALSE);App::stemProc(app->stemWindow,WM_PAINT,0,0);paints++;
                    for(auto widget:app->stemWidgets){auto child=GetDlgItem(app->stemWindow,widget.id);InvalidateRect(child,nullptr,FALSE);App::stemControlProc(child,WM_PAINT,0,0,widget.id,(DWORD_PTR)app.get());}
                    require(app->stemPendingError.empty(),"Paint callback accepts malformed result state");
                }}
            }
            auto child=app->stemWindow;app->stems.running=true;app->stemEnable(false);require(IsWindowEnabled(GetDlgItem(child,742)),"Close remains available during separation");App::stemProc(child,WM_COMMAND,742,0);require(!app->stemLabTab&&app->editorVisible&&app->mode==4&&!app->stems.cancel&&app->stems.running,"Close button returns to Sample Lab without cancelling job");app->stems.running=false;app->stemEnable(true);app->openStems();require(child==app->stemWindow&&app->stemLabTab,"Panel reopens without recreation");DestroyWindow(app->stemWindow);
        }
        app->stemPreferences=Json::object();app->openStems();require(app->stemWindow&&app->stemPendingError.empty(),"Panel before callback exception check");
        app->selected=TrackCount+10;App::stemProc(app->stemWindow,WM_COMMAND,702,0);app->selected=0;
        require(!app->stemPendingError.empty()&&!app->stemLabTab,"Callback exception contained");require(projectJson(app->project,app->root/L"unchanged.vamp")==before,"Project data unchanged");
        app->stemPendingError.clear();app->openStems();require(app->stemLabTab&&app->stemPendingError.empty(),"Panel recovers after callback error");
        struct Preview{const wchar_t*name;int width,height,tab,language;double scale;};
        for(auto sample:std::array<Preview,5>{{{L"separation-en.png",960,540,0,0,1},{L"separation-ru.png",960,540,0,1,1},{L"fine-tuning-ru.png",720,460,1,1,1},{L"compact-ru.png",460,340,0,1,.85},{L"compact-fine-ru.png",460,340,1,1,.85}}}){
            // Recreate to translate the actual native control strings as well.
            DestroyWindow(app->stemWindow);ui::language=sample.language;app->scale=sample.scale;app->openStems();app->stemSettingsTab=sample.tab;
            SetWindowPos(app->stemWindow,nullptr,0,0,sample.width,sample.height,SWP_NOZORDER|SWP_NOACTIVATE);app->layoutStemControls();app->stemSetText(700,L"Nightfall.wav · 00:24");auto file=reportPath.parent_path()/L"stem-previews"/sample.name;app->saveStemPreview(file);previews.push_back(utf8(file.wstring()));
        }
        report.update({{"passed",true},{"native_panels_created",panels+6},{"actual_paint_callbacks",paints},{"layout_cases",layouts},{"closed_combo_wheel_checks",wheels},{"no_scrollbars",true},{"native_bounds_and_overlap",true},{"open_close_reopen",true},{"callback_exception_contained",true},{"project_unchanged",true},{"shared_renderer_previews",previews}});
    }catch(const std::exception&e){report["error"]=e.what();}
    if(app->window)DestroyWindow(app->window);app.reset();fs::create_directories(reportPath.parent_path());std::ofstream file(reportPath);file<<report.dump(2);bool ok=report.at("passed")==true&&bool(file);file.close();LocalFree(argv);CoUninitialize();return ok?0:1;
}
