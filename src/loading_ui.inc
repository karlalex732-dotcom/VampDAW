    struct LoadingTask{uint64_t id=0;std::wstring title,detail;double progress=-1;};
    std::vector<LoadingTask>loadingTasks;uint64_t loadingId=0;bool loadingWasActive=false;ULONGLONG lastLoadingPaint=0;
    std::vector<LoadingTask>currentLoadingTasks(){auto tasks=loadingTasks;
        if(projectImporter.busy)tasks.push_back({0,L"Importing project · Esc cancels",projectImporter.status(),projectImporter.progress.load()});
        if(audioImportWorker.joinable()&&!audioImportDone)tasks.push_back({0,L"Loading audio",activeAudioImport.source.filename().wstring(),-1});
        if(!catalogDone)tasks.push_back({0,L"Scanning plugins",number(catalogScanned)+L" / "+number(catalogTotal),catalogTotal?catalogScanned/double(catalogTotal):-1});
        if(warpBusy)tasks.push_back({0,L"Warp audio",number(warpCompleted.load())+L" / "+number(warpJobs.size()),warpJobs.size()>1?warpCompleted.load()/double(warpJobs.size()):-1});
        if(sampleBusy)tasks.push_back({0,L"Processing audio",L"Sample Lab",-1});
        if(stems.running)tasks.push_back({0,L"Blood Stems",stemStatusText(true,stemResult,stems.status()),stems.progress.load()});
        if(exportBusy)tasks.push_back({0,bounceRequest?L"Bounce in place":folderFreezeActive?L"Freezing folder":freezeExportTrack>=0?L"Freezing track":L"Export",folderFreezeActive?folderFreezeName+L" · "+number(folderFreezeCompleted+1)+L" / "+number(folderFreezeTotal):exportPath.filename().wstring(),folderFreezeActive?(folderFreezeCompleted+exportControl.progress.load())/std::max(1,folderFreezeTotal):exportControl.phase==1?-1:exportControl.progress.load()});
        for(auto&chain:plugins)for(auto&plug:chain){double progress=plug->araProgress();if(progress>=0)tasks.push_back({0,L"ARA analysis",wide(plug->info.name),progress});}
        return tasks;
    }
    bool paintLoadingFooter(Painter&p,Rect area){
        auto tasks=currentLoadingTasks();if(tasks.empty())return false;
        size_t index=(GetTickCount64()/3500)%tasks.size();auto&t=tasks[index];std::wstring label=ui::translate(t.title);
        if(!t.detail.empty())label+=L" · "+t.detail;
        if(tasks.size()>1)label=number(index+1)+L"/"+number(tasks.size())+L"  "+label;
        float percentWidth=t.progress<0?24:47;
        p.text(label,{area.x+6,area.y,area.w-percentWidth-13,21},9,textColor,false,1,false);
        p.text(t.progress<0?L"…":number(clamp(t.progress,0,1)*100)+L"%",{area.x+area.w-percentWidth,area.y,percentWidth-3,21},9,crimson,true,2);
        Rect rail{area.x+6,area.y+23,std::max(1.f,area.w-12),2};p.fill(rail,lineColor,1);
        float length=t.progress<0?rail.w*.18f:rail.w*(float)clamp(t.progress,0,1),x=rail.x;
        if(t.progress<0){double phase=animationLevel?GetTickCount64()%1800/1800.:.35;x+=(rail.w-length)*(float)(1-std::abs(2*phase-1));}
        p.fill({x,rail.y,length,2},crimson,1);return true;
    }
    RECT loadingFooterRect(const RECT&client)const{
        return {std::max(0L,(LONG)std::floor(-canvasScrollX)),
            std::max(0L,(LONG)std::floor((height-28+26)*scale-canvasScrollY-1)),
            std::min(client.right,(LONG)std::ceil(width*scale-canvasScrollX)),
            std::min(client.bottom,(LONG)std::ceil((height+26)*scale-canvasScrollY))};
    }
    bool paintLoadingUpdate(HDC target){
        RECT client{};if(!GetClientRect(window,&client))return false;auto bounds=loadingFooterRect(client);
        WindowPaintRegion buffer(target,bounds);if(!buffer.valid())return false;
        {Graphics g(buffer.dc);g.Clear(bg);Matrix transform(scale,0,0,scale,-canvasScrollX-bounds.left,26*scale-canvasScrollY-bounds.top);g.SetTransform(&transform);Painter p(g,fonts);footer(p);}
        return true;
    }
    void refreshLoading(){
        if(!window||!IsWindowVisible(window)||IsIconic(window)||scale<=0)return;
        bool active=!currentLoadingTasks().empty();if(!active&&!loadingWasActive)return;
        bool transition=active!=loadingWasActive;auto now=GetTickCount64();
        if(!transition&&now-lastLoadingPaint<33)return;
        // Synchronous jobs cannot pump messages safely. Present a completed strip,
        // never its background/text/progress as separate visible drawing commands.
        HDC dc=GetDC(window);if(!dc)return;bool painted=paintLoadingUpdate(dc);ReleaseDC(window,dc);
        if(painted){loadingWasActive=active;lastLoadingPaint=now;}else{RECT client{};GetClientRect(window,&client);auto bounds=loadingFooterRect(client);InvalidateRect(window,&bounds,FALSE);}
    }
    struct LoadingOperation{App&app;uint64_t id;LoadingOperation(App&a,const std::wstring&title,const std::wstring&detail=L""):app(a),id(++a.loadingId){app.loadingTasks.push_back({id,title,detail,-1});app.refreshLoading();}~LoadingOperation(){app.loadingTasks.erase(std::remove_if(app.loadingTasks.begin(),app.loadingTasks.end(),[&](const auto&t){return t.id==id;}),app.loadingTasks.end());app.refreshLoading();}void update(const std::wstring&detail,double progress=-1){for(auto&t:app.loadingTasks)if(t.id==id){t.detail=detail;t.progress=progress;}app.refreshLoading();}LoadingOperation(const LoadingOperation&)=delete;};
