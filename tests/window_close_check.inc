if(argc>2&&std::wstring(argv[1])==L"--window-close-check"){
    auto path=fs::absolute(argv[2]);Json report={{"passed",false},{"desktop_interaction_tested",false}};auto app=std::make_unique<App>();
    try{
        auto require=[](bool ok,const char*why){if(!ok)throw std::runtime_error(why);};
        app->root=path.parent_path()/L"window-close-data";app->stateRoot=app->root/L"settings";fs::create_directories(app->stateRoot);
        GdiplusStartupInput input;require(GdiplusStartup(&app->gdipToken,&input,nullptr)==Ok,"GDI+ startup");
        app->window=CreateWindowExW(0,L"STATIC",L"VampDAW internal lifecycle test",WS_OVERLAPPEDWINDOW,0,0,1200,800,nullptr,nullptr,instance,nullptr);
        require(app->window!=nullptr,"Parent window");auto original=projectJson(app->project,app->root/L"project.vamp");int closes=0;
        auto unchanged=[&]{require(IsWindow(app->window)&&!IsIconic(app->window),"Closing a panel must retain the main window");MSG message{};require(!PeekMessageW(&message,nullptr,WM_QUIT,WM_QUIT,PM_NOREMOVE),"Panel close must not quit");};
        auto create=[&](WNDPROC proc,const wchar_t*name){WNDCLASSW cls{};cls.hInstance=instance;cls.lpfnWndProc=proc;cls.lpszClassName=name;RegisterClassW(&cls);auto h=CreateWindowExW(0,name,L"Internal panel test",WS_OVERLAPPEDWINDOW,0,0,900,650,app->window,nullptr,instance,app.get());require(h!=nullptr,"Auxiliary window creation");return h;};
        // Real window callbacks, without enumerating audio devices or starting playback.
        for(int pass=0;pass<2;pass++){
            app->pluginPickerWindow=create(App::pluginPickerProc,L"VampDAW.Test.Picker");auto h=app->pluginPickerWindow;SendMessageW(h,WM_CLOSE,0,0);require(IsWindow(h)&&!(GetWindowLongPtrW(h,GWL_STYLE)&WS_VISIBLE),"Picker hides");unchanged();DestroyWindow(h);closes++;
            app->poolWindow=create(App::poolProc,L"VampDAW.Test.Pool");h=app->poolWindow;app->poolDragging=true;SendMessageW(h,pass?WM_KEYDOWN:WM_CLOSE,pass?VK_ESCAPE:0,0);if(app->poolDragging||pass)SendMessageW(h,WM_CLOSE,0,0);require(!app->poolDragging&&IsWindow(h),"Pool closes and ends drag");unchanged();DestroyWindow(h);closes++;
            app->deviceWindow=create(App::deviceProc,L"VampDAW.Test.Devices");h=app->deviceWindow;SendMessageW(h,pass?WM_COMMAND:WM_CLOSE,pass?106:0,0);require(!IsWindow(h)&&!app->deviceWindow,"Devices close and release state");unchanged();closes++;
            app->aggregateWindow=create(App::aggregateProc,L"VampDAW.Test.Aggregate");h=app->aggregateWindow;SendMessageW(h,pass?WM_COMMAND:WM_CLOSE,pass?241:0,0);require(!IsWindow(h)&&!app->aggregateWindow,"Aggregate closes and releases state");unchanged();closes++;
            app->openControllers(false);h=app->controllerWindow;SendMessageW(h,pass?WM_COMMAND:WM_CLOSE,pass?1213:0,0);require(!IsWindow(h)&&!app->controllerWindow,"Controllers close and release state");unchanged();closes++;
            app->themeBeforeIndex=app->uiTheme;app->themeBefore=app->customTheme;app->themeWindow=create(App::themeProc,L"VampDAW.Test.Theme");h=app->themeWindow;SendMessageW(h,pass?WM_COMMAND:WM_CLOSE,pass?963:0,0);require(!IsWindow(h)&&!app->themeWindow,"Theme closes");unchanged();closes++;
            app->openStems();h=app->stemWindow;SendMessageW(h,pass?WM_KEYDOWN:WM_CLOSE,pass?VK_ESCAPE:0,0);require(!app->stemLabTab&&IsWindow(h),"Blood Stems closes");unchanged();DestroyWindow(h);closes++;
        }
        WNDCLASSW fxClass{};fxClass.hInstance=instance;fxClass.lpfnWndProc=NativeEffect::windowProc;fxClass.lpszClassName=L"VampDAW.Test.NativeEffect";RegisterClassW(&fxClass);int effects=0;
        for(int kind=0;kind<(int)nativeSpecs().size();kind++){
            NativeEffect fx(kind,48000);auto h=CreateWindowExW(0,fxClass.lpszClassName,L"Internal effect lifecycle test",WS_OVERLAPPEDWINDOW,0,0,1000,760,app->window,nullptr,instance,&fx);require(h!=nullptr,"Native effect creation");
            for(UINT method:{WM_CLOSE,WM_KEYDOWN}){if(kind==0)fx.analyzerRunning=true;SendMessageW(h,method,method==WM_KEYDOWN?VK_ESCAPE:0,0);require(IsWindow(h)&&!(GetWindowLongPtrW(h,GWL_STYLE)&WS_VISIBLE),"Effect close hides editor");if(kind==0)require(!fx.analyzerRunning,"Closing EQ stops its analyzer");unchanged();closes++;}
            installPanelEscape(h);if(fx.assistantSlider){SendMessageW(fx.assistantSlider,WM_KEYDOWN,VK_ESCAPE,0);unchanged();closes++;}DestroyWindow(h);require(fx.window==nullptr,"Destroyed effect editor clears handle");effects++;
        }
        for(int pane=0;pane<=9;pane++){app->closePane(pane);unchanged();closes++;}app->closeAllPanes();unchanged();
        require(projectJson(app->project,app->root/L"project.vamp")==original,"Closing windows preserves project data");
        report.update({{"passed",true},{"close_callbacks",closes},{"native_effect_editors",effects},{"main_window_preserved",true},{"no_quit_message",true},{"project_unchanged",true},{"auxiliary_recreation",true}});
    }catch(const std::exception&e){report["error"]=e.what();}
    if(app->window)DestroyWindow(app->window);app.reset();fs::create_directories(path.parent_path());std::ofstream file(path);file<<report.dump(2);bool ok=report.at("passed")==true&&bool(file);file.close();LocalFree(argv);CoUninitialize();return ok?0:1;
}
