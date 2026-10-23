if(argc>2&&std::wstring(argv[1])==L"--loading-footer-check"){
    auto path=fs::absolute(argv[2]);Json report={{"passed",false},{"desktop_interaction_tested",false}};auto app=std::make_unique<App>();
    try{
        auto require=[](bool ok,const char*why){if(!ok)throw std::runtime_error(why);};
        struct Surface{
            HDC dc=nullptr;HBITMAP bitmap=nullptr,old=nullptr;uint32_t*pixels=nullptr;int w,h;
            Surface(int width,int height):w(width),h(height){auto screen=GetDC(nullptr);dc=CreateCompatibleDC(screen);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,(void**)&pixels,nullptr,0);ReleaseDC(nullptr,screen);if(!dc||!bitmap)throw std::runtime_error("DIB surface allocation");old=(HBITMAP)SelectObject(dc,bitmap);}
            ~Surface(){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}
            void clear(){GdiFlush();std::fill(pixels,pixels+w*h,0x00591783u);}
        };
        {Surface target(100,80);target.clear();RECT bounds{10,20,90,70};
            {WindowPaintRegion region(target.dc,bounds);require(region.valid(),"Off-screen region allocates");RECT local{0,0,80,50};auto brush=CreateSolidBrush(RGB(200,30,70));FillRect(region.dc,&local,brush);DeleteObject(brush);GdiFlush();for(int i=0;i<target.w*target.h;i++)require((target.pixels[i]&0xffffff)==0x591783,"Drawing an unfinished frame never touches the destination");}
            GdiFlush();for(int y=0;y<80;y++)for(int x=0;x<100;x++){bool inside=x>=10&&x<90&&y>=20&&y<70;require((target.pixels[y*100+x]&0xffffff)==(inside?0xc81e46u:0x591783u),"Completed region commits once, within its bounds");}
        }
        app->root=path.parent_path()/L"loading-footer-data";app->stateRoot=app->root/L"settings";fs::create_directories(app->stateRoot);GdiplusStartupInput startup;require(GdiplusStartup(&app->gdipToken,&startup,nullptr)==Ok,"GDI+ startup");app->applyTheme();app->catalogDone=true;app->animationLevel=0;
        app->window=CreateWindowExW(0,L"STATIC",L"Hidden loading footer regression",WS_OVERLAPPEDWINDOW,0,0,1440,900,nullptr,nullptr,instance,nullptr);require(app->window!=nullptr,"Hidden parent");
        int cases=0;size_t checked=0;int maximumDifference=0;
        for(float zoom:{.8f,1.f,1.25f,1.5f,2.f})for(int language:{0,1})for(int animation:{0,2}){
            int w=zoom>=1.5f?2560:zoom>=1.25f?1920:1440,h=zoom>=1.5f?1440:900;RECT outer{0,0,w,h};AdjustWindowRect(&outer,WS_OVERLAPPEDWINDOW,FALSE);SetWindowPos(app->window,nullptr,0,0,outer.right-outer.left,outer.bottom-outer.top,SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
            RECT actual{};require(GetClientRect(app->window,&actual),"Actual client rectangle");w=actual.right;h=actual.bottom;
            app->scale=zoom;app->width=w/zoom;app->height=h/zoom-26;app->canvasScrollX=cases%2?11:0;app->canvasScrollY=cases%2?7:0;ui::language=language;app->animationLevel=animation;
            Surface target(w,h),reference(w,h);RECT client{0,0,w,h};auto bounds=app->loadingFooterRect(client);
            for(int step=0;step<8;step++){
                if(step==7){app->loadingTasks.clear();app->status=L"Ready / completed";}
                else app->loadingTasks={{1,L"Loading audio",L"sample.wav",step==6?-1:step/5.}};
                // Stop time-dependent movement so the independent full-frame render is comparable.
                app->animationLevel=0;target.clear();
                {Graphics g(reference.dc);g.Clear(bg);g.ScaleTransform(zoom,zoom);g.TranslateTransform(-app->canvasScrollX/zoom,26-app->canvasScrollY/zoom);Painter p(g,app->fonts);app->footer(p);}
                require(app->paintLoadingUpdate(target.dc),"Status update uses the buffered region");GdiFlush();int changed=0;
                for(int y=0;y<h;y++)for(int x=0;x<w;x++){
                    auto pixel=target.pixels[y*w+x]&0xffffff;
                    if(x<bounds.left||x>=bounds.right||y<bounds.top||y>=bounds.bottom){require(pixel==0x591783,"A loading update never repaints the workspace");continue;}
                    auto expected=reference.pixels[y*w+x]&0xffffff;changed+=pixel!=0x591783;
                    for(int shift:{0,8,16})maximumDifference=std::max(maximumDifference,std::abs(int((pixel>>shift)&255)-int((expected>>shift)&255)));checked++;
                }
                require(changed>100,"Footer is fully initialized, including task completion");
            }
            cases++;
        }
        require(maximumDifference<=2,"Buffered strip agrees with the normal full-frame footer at every DPI");
        app->canvasScrollX=app->canvasScrollY=0;app->loadingTasks={{1,L"Loading audio",L"example.wav",.65}};Surface repeated(2560,1440);require(app->paintLoadingUpdate(repeated.dc),"Warm up font and painting resources");auto resources=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        for(int i=0;i<200;i++){app->loadingTasks[0].progress=(i%101)/100.;require(app->paintLoadingUpdate(repeated.dc),"Rapid loading update");}
        auto finalResources=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);require(finalResources<=resources+2,"Rapid status updates release their GDI resources");
        app->loadingTasks[0].progress=.65;app->paintLoadingUpdate(repeated.dc);GdiFlush();Bitmap full(repeated.bitmap,nullptr);auto bounds=app->loadingFooterRect({0,0,2560,1440});std::unique_ptr<Bitmap>preview(full.Clone((INT)bounds.left,(INT)bounds.top,(INT)(bounds.right-bounds.left),(INT)(bounds.bottom-bounds.top),PixelFormat32bppARGB));
        UINT count=0,bytes=0;GetImageEncodersSize(&count,&bytes);std::vector<BYTE>storage(bytes);auto codecs=(ImageCodecInfo*)storage.data();GetImageEncoders(count,bytes,codecs);auto image=path.parent_path()/L"loading-footer.png";fs::create_directories(path.parent_path());for(UINT i=0;i<count;i++)if(!wcscmp(codecs[i].MimeType,L"image/png"))require(preview->Save(image.c_str(),&codecs[i].Clsid,nullptr)==Ok,"Footer preview saves");
        report.update({{"passed",true},{"destination_unchanged_until_commit",true},{"workspace_untouched",true},{"layouts",cases},{"frames",cases*8},{"pixels_compared",checked},{"maximum_channel_difference",maximumDifference},{"rapid_updates",200},{"gdi_objects_before",resources},{"gdi_objects_after",finalResources},{"preview",utf8(image.wstring())}});
    }catch(const std::exception&e){report["error"]=e.what();}
    if(app->window)DestroyWindow(app->window);app->window=nullptr;app.reset();fs::create_directories(path.parent_path());std::ofstream(path)<<report.dump(2);bool passed=report.value("passed",false);LocalFree(argv);CoUninitialize();return passed?0:1;
}
