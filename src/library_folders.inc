    std::array<int,2>folderSelected{-1,-1},folderScroll{0,0};
    std::array<Rect,2>folderLists{};int folderVisible=6,folderFocus=0;
    void folderSettings(Painter&p,float x,float y,float w){
        float column=(w-20)/2;folderVisible=6;
        for(int kind=0;kind<2;kind++){
            auto&roots=kind?pluginRoots:soundRoots;if(folderSelected[kind]<0&&!roots.empty())folderSelected[kind]=0;float xx=x+kind*(column+20);
            p.text(std::wstring(kind?L"User Plugins":L"User Sounds")+L" / "+number(roots.size()),{xx,y,column-40,28},16,textColor,true);
            button(p,L"+",{xx+column-34,y,34,27},kind?AddPluginFolder:AddSoundFolder);
            folderScroll[kind]=(int)clamp(folderScroll[kind],0,std::max(0,(int)roots.size()-folderVisible));
            folderLists[kind]={xx,y+38,column,folderVisible*35.f};p.fill(folderLists[kind],bg,5);
            for(int row=0;row<folderVisible;row++){int index=folderScroll[kind]+row;if(index>=(int)roots.size())break;Rect r{xx+4,y+42+row*35,column-8,31};
                if(index==folderSelected[kind])p.fill(r,raised,3);p.text(number(index+1)+L"  "+roots[index].wstring(),{r.x+6,r.y,r.w-28,r.h},10,index==folderSelected[kind]?textColor:muted,false,0,false);addHit(FolderSelect,r,kind,index);
            }
            navBar(p,{xx+column-15,y+42,11,folderVisible*35.f-8},8+kind,folderScroll[kind],std::max(folderVisible,(int)roots.size()),folderVisible,true);
            float by=y+262;button(p,L"Edit…",{xx,by,89,30},FolderEdit,false,kind);button(p,L"Remove",{xx+95,by,84,30},FolderRemove,false,kind);
            button(p,L"↑",{xx+185,by,35,30},FolderUp,false,kind);button(p,L"↓",{xx+226,by,35,30},FolderDown,false,kind);
            button(p,L"‹",{xx+column-69,by,30,30},FolderPage,false,kind,-1);button(p,L"›",{xx+column-34,by,30,30},FolderPage,false,kind,1);
            int index=folderSelected[kind];if(index>=0&&index<(int)roots.size()){auto value=roots[index].wstring();size_t chars=(size_t)std::max(18.f,column/5.7f);for(int line=0;line<3&&line*chars<value.size();line++)p.text(value.substr(line*chars,chars),{xx,y+307+line*21,column,21},10,muted,false,0,false);}
        }
        p.text(L"Remove only disconnects the folder. Files stay on disk.",{x,y+384,w,25},10,muted);
        button(p,L"Rescan plugins",{x,y+420,240,32},RescanCatalog);button(p,L"Find DAW folders",{x+252,y+420,std::min(230.f,w-252),32},DiscoverPluginFolders);
    }
    void folderAction(Action action,int kind,int row=-1){
        if(kind<0||kind>1)return;auto&roots=kind?pluginRoots:soundRoots;auto&selectedFolder=folderSelected[kind];
        if(action==FolderSelect){folderFocus=kind;selectedFolder=row;InvalidateRect(window,nullptr,FALSE);return;}
        if(action==FolderPage){folderScroll[kind]=(int)clamp(folderScroll[kind]+row*folderVisible,0,std::max(0,(int)roots.size()-folderVisible));InvalidateRect(window,nullptr,FALSE);return;}
        if(selectedFolder<0||selectedFolder>=(int)roots.size()){notify(L"Select a folder in the list first");return;}
        if(action==FolderEdit){auto value=TextPrompt::show(window,ui::translate(L"Folder path"),roots[selectedFolder].wstring());if(!value)return;std::error_code ec;fs::path path=fs::absolute(fs::path(*value),ec).lexically_normal();if(!fs::is_directory(path,ec)){notify(L"Folder not found");return;}for(size_t i=0;i<roots.size();i++)if((int)i!=selectedFolder&&lower(roots[i].lexically_normal().wstring())==lower(path.lexically_normal().wstring())){notify(L"This folder is already in the list");return;}roots[selectedFolder]=path;}
        else if(action==FolderRemove){roots.erase(roots.begin()+selectedFolder);selectedFolder=std::min(selectedFolder,(int)roots.size()-1);}
        else if(action==FolderUp||action==FolderDown){int to=selectedFolder+(action==FolderUp?-1:1);if(to<0||to>=(int)roots.size())return;std::swap(roots[selectedFolder],roots[to]);selectedFolder=to;}
        if(!kind){browserRoot=(int)clamp(browserRoot,0,std::max(0,(int)soundRoots.size()-1));browserPath=soundRoots.empty()?fs::path{}:soundRoots[browserRoot];}
        folderScroll[kind]=(int)clamp(folderScroll[kind],std::max(0,selectedFolder-folderVisible+1),std::max(0,selectedFolder));saveSettings();refreshBrowser();InvalidateRect(window,nullptr,FALSE);
    }
