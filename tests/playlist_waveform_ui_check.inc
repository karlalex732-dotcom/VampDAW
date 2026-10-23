if(argc>2&&std::wstring(argv[1])==L"--playlist-waveform-check"){
    auto path=fs::absolute(argv[2]);Json report={{"passed",false},{"desktop_interaction_tested",false}};auto app=std::make_unique<App>();
    try{
        auto require=[](bool ok,const char*why){if(!ok)throw std::runtime_error(why);};
        app->root=path.parent_path()/L"waveform-ui-data";app->stateRoot=app->root/L"settings";fs::create_directories(app->stateRoot);
        GdiplusStartupInput input;require(GdiplusStartup(&app->gdipToken,&input,nullptr)==Ok,"GDI+ startup");
        app->window=CreateWindowExW(0,L"STATIC",L"Hidden waveform UI check",WS_OVERLAPPEDWINDOW,0,0,1440,900,nullptr,nullptr,instance,nullptr);
        require(app->window!=nullptr,"Hidden parent");app->applyTheme();ui::language=0;app->introDismissed=true;app->mixerDockVisible=false;
        app->width=1440;app->height=900;app->work={24,168,1380,698};app->mode=0;app->project=Project{};app->project.bpm=120;app->project.tracks.resize(1);
        auto quiet=std::make_shared<Sample>();quiet->rate=48000;quiet->path=L"Quiet stereo.wav";quiet->left.resize(96000);quiet->right.resize(96000);
        for(int i=0;i<96000;i++){quiet->left[i]=(float)(.125*std::sin(i*6.283185307179586*220/48000));quiet->right[i]=(float)(.125*std::sin(i*6.283185307179586*330/48000));}
        auto&track=app->project.tracks[0];track.name="Quiet stereo";track.sample=quiet;track.clips={{0,4}};
        std::shared_ptr<const PlaylistWaveform>ready;auto deadline=GetTickCount64()+3000;
        while(!(ready=app->playlistWaveforms.get(quiet,app->window))&&GetTickCount64()<deadline)Sleep(2);
        require(ready&&std::abs(ready->peak-.125)<1e-6,"Quiet audio has a reusable peak reference");
        Sample impulse;impulse.left.resize(250003);impulse.right.resize(250003);impulse.left[12345]=1;impulse.right[200007]=-.9f;
        auto peaks=PlaylistWaveform::build(impulse);auto all=peaks->range(impulse,0,impulse.left.size());
        require(all.high==1&&std::abs(all.low+.9f)<1e-7,"Overview keeps one-frame impulses in either channel");
        for(size_t i=0;i<200;i++){size_t first=(i*7919)%impulse.left.size(),last=std::min(impulse.left.size(),first+(i*1237)%150000+1);auto exact=waveformFrames(impulse,first,last),fast=peaks->range(impulse,first,last);require(exact.valid==fast.valid&&exact.low==fast.low&&exact.high==fast.high,"Zoom ranges preserve exact stereo extrema");}
        std::atomic<bool>cancel{true};require(!PlaylistWaveform::build(impulse,&cancel),"Background waveform analysis is cancellable");
        Json previews=Json::array(),coverage=Json::array();int cases=0;
        auto canvas=[&](const std::wstring&name){
            app->hits.clear();app->navBars.clear();Bitmap bitmap(1440,900,PixelFormat32bppPARGB);
            {Graphics g(&bitmap);Painter p(g,app->fonts);p.fill({0,0,1440,900},bg);app->header(p);app->playlist(p);}
            float row=app->playlistLayout.height(0);float visible=std::min(app->arrangement.w,app->arrangement.w/(float)app->visibleBars);
            int x=(int)(app->arrangement.x+visible*.65),top=900,bottom=-1;
            for(int xx=x;xx<std::min(1440,x+28);xx++)for(int yy=(int)app->arrangement.y+4;yy<(int)(app->arrangement.y+row-3);yy++){
                Color pixel;bitmap.GetPixel(xx,yy,&pixel);if(pixel.GetR()>100&&pixel.GetR()>pixel.GetG()*1.5&&pixel.GetR()>pixel.GetB()*1.4){top=std::min(top,yy);bottom=std::max(bottom,yy);}
            }
            require(bottom-top>=row-13,"Waveform uses the current row height at this zoom");coverage.push_back({{"height",row},{"visible_bars",app->visibleBars},{"waveform_pixels",bottom-top}});
            UINT n=0,size=0;GetImageEncodersSize(&n,&size);std::vector<BYTE>data(size);auto codecs=(ImageCodecInfo*)data.data();GetImageEncoders(n,size,codecs);auto file=path.parent_path()/L"waveform-previews"/(name+L".png");fs::create_directories(file.parent_path());
            for(UINT i=0;i<n;i++)if(!wcscmp(codecs[i].MimeType,L"image/png"))require(bitmap.Save(file.c_str(),&codecs[i].Clsid,nullptr)==Ok,"PNG save");
            previews.push_back(utf8(file.wstring()));cases++;
        };
        for(int height:{32,96,224})for(double bars:{8.,1.,.25}){track.playlistHeight=(float)height;app->visibleBars=bars;canvas(std::to_wstring(height)+L"-"+number(bars,2));}
        track.playlistHeight=96;app->visibleBars=8;canvas(L"before-drag");
        auto resize=std::find_if(app->hits.begin(),app->hits.end(),[](auto&h){return h.action==PlaylistTrackResize&&h.extra==0;});require(resize!=app->hits.end(),"Track resize handle");auto hit=*resize;
        app->mouseDown(hit.rect.x+10,hit.rect.y+3);app->mouseMove(hit.rect.x+10,hit.rect.y+93);app->mouseUp();require(track.playlistHeight==186,"Actual row drag changes height");canvas(L"after-drag");
        auto zoom=std::find_if(app->hits.begin(),app->hits.end(),[](auto&h){return h.action==ZoomIn;});require(zoom!=app->hits.end(),"Playlist zoom button");auto z=*zoom;app->mouseDown(z.rect.x+8,z.rect.y+8);app->mouseUp();require(app->visibleBars<8,"Actual zoom command updates the view");canvas(L"after-zoom");
        report.update({{"passed",true},{"layout_cases",cases},{"range_queries",200},{"single_frame_and_right_channel_peaks",true},{"quiet_audio_display_normalization",true},{"row_resize_mouse_callback",true},{"zoom_button_callback",true},{"coverage",coverage},{"previews",previews}});
    }catch(const std::exception&e){report["error"]=e.what();}
    if(app->window)DestroyWindow(app->window);app->window=nullptr;app.reset();fs::create_directories(path.parent_path());std::ofstream(path)<<report.dump(2);bool passed=report.value("passed",false);LocalFree(argv);CoUninitialize();return passed?0:1;
}
