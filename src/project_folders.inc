    struct ProjectRow{int id,depth;bool folder;};std::vector<ProjectRow>projectRows;
    PlaylistLayout playlistLayout;
    void layoutPlaylistRows(){std::vector<float>heights;for(auto&r:projectRows)heights.push_back(r.folder?(trackFolder(r.id)->playlistHeight>0?trackFolder(r.id)->playlistHeight:(float)clamp(trackHeight,24,40)):project.tracks[r.id].playlistHeight>0?project.tracks[r.id].playlistHeight:trackHeight);playlistLayout.reset(heights,arrangement.h);playlistRowOffset=std::clamp(playlistRowOffset,0,playlistLayout.maxOffset());}
    int playlistRowOffset=0,projectTreeScroll=0,projectTreeFocus=-1,projectTreeFolder=-1;Rect projectTreeList{};
    TrackFolder*trackFolder(int id){for(auto&f:project.folders)if(f.id==id)return &f;return nullptr;}
    void refreshProjectRows(){
        projectRows.clear();std::function<void(int,int)>append=[&](int parent,int depth){
            if(depth>32)return;
            for(auto&f:project.folders)if(f.parent==parent){projectRows.push_back({f.id,depth,true});if(!f.collapsed)append(f.id,depth+1);}
            for(int i=0;i<(int)project.tracks.size();i++)if(project.tracks[i].folder==parent)projectRows.push_back({i,depth,false});
        };append(-1,0);playlistRowOffset=(int)clamp(playlistRowOffset,0,std::max(0,(int)projectRows.size()-1));
    }
    int playlistTrackAt(int row)const{int pos=playlistRowOffset+row;return pos>=0&&pos<(int)projectRows.size()&&!projectRows[pos].folder?projectRows[pos].id:-1;}
    int playlistTrackAtY(float y)const{int row=playlistLayout.hit(y-arrangement.y,playlistRowOffset);return row>=0&&!projectRows[row].folder?projectRows[row].id:-1;}
    int playlistRowIndex(int track)const{for(int i=0;i<(int)projectRows.size();i++)if(!projectRows[i].folder&&projectRows[i].id==track)return i;return -1;}
    void revealPlaylistTrack(int track){
        if(track==MasterChannel)return;int id=project.tracks[track].folder;for(int depth=0;id>=0&&depth<32;depth++){auto*f=trackFolder(id);if(!f)break;f->collapsed=false;id=f->parent;}
        refreshProjectRows();layoutPlaylistRows();int row=playlistRowIndex(track);if(row>=0&&(row<playlistRowOffset||row>=playlistLayout.end(playlistRowOffset)))playlistRowOffset=(int)clamp(row,0,playlistLayout.maxOffset());
    }
    ClipSelection playlistRectangle(double a,double b,double top,double bottom){
        ClipSelection result;int first=(int)std::floor(std::min(top,bottom)),last=(int)std::floor(std::max(top,bottom));
        for(int row=std::max(0,first);row<=last&&row<(int)projectRows.size();row++)if(!projectRows[row].folder){int track=projectRows[row].id;for(int i=0;i<(int)project.tracks[track].clips.size();i++){auto&c=project.tracks[track].clips[i];if(c.beat+c.length>=std::min(a,b)&&c.beat<=std::max(a,b))result.insert({track,i});}}return result;
    }
    std::wstring projectFolderPath(int id){std::wstring result;for(int n=0;id>=0&&n<32;n++){auto*f=trackFolder(id);if(!f)break;result=wide(f->name)+(result.empty()?L"":L" / "+result);id=f->parent;}return result;}
    std::set<int>folderTracks(int id){std::set<int>result;for(int i=0;i<(int)project.tracks.size();i++){int parent=project.tracks[i].folder;for(int depth=0;parent>=0&&depth<32;depth++){if(parent==id){result.insert(i);break;}auto*f=trackFolder(parent);if(!f)break;parent=f->parent;}}return result;}
    int chooseProjectFolder(int exclude=-1){
        HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Project root");
        for(auto&f:project.folders){bool allowed=f.id!=exclude;int parent=f.parent;for(int n=0;allowed&&parent>=0&&n<32;n++){if(parent==exclude){allowed=false;break;}auto*p=trackFolder(parent);if(!p)break;parent=p->parent;}if(allowed)AppendMenuW(menu,MF_STRING,2+f.id,projectFolderPath(f.id).c_str());}
        POINT pt;GetCursorPos(&pt);int id=TrackPopupMenu(menu,TPM_RETURNCMD,pt.x,pt.y,0,window,nullptr);DestroyMenu(menu);return id==0?-2:id==1?-1:id-2;
    }
    void newProjectFolder(int parent=-1,bool group=true){
        auto name=TextPrompt::show(window,L"Folder name",L"Folder");if(!name||name->empty())return;
        try{auto candidate=project;int id=createTrackFolder(candidate,utf8(*name),parent);if(group&&selected!=MasterChannel)assignTracksToFolder(candidate,mixerTargets(selected),id);history();project=std::move(candidate);projectTreeFolder=id;leftPanelVisible=browserVisible=true;browserTab=3;refreshProjectRows();changed();}
        catch(const std::exception&e){error(e.what());}
    }
    void groupTracksMenu(){
        HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"New folder from selected tracks…");AppendMenuW(menu,MF_STRING,2,L"Move selected tracks to folder…");AppendMenuW(menu,MF_STRING,3,L"Move selected tracks to project root");POINT pt;GetCursorPos(&pt);int id=TrackPopupMenu(menu,TPM_RETURNCMD,pt.x,pt.y,0,window,nullptr);DestroyMenu(menu);if(!id||selected==MasterChannel)return;
        if(id==1){newProjectFolder(-1,true);return;}int dest=id==3?-1:chooseProjectFolder();if(dest< -1)return;
        try{auto candidate=project;assignTracksToFolder(candidate,mixerTargets(selected),dest);history();project=std::move(candidate);refreshProjectRows();changed();}catch(const std::exception&e){error(e.what());}
    }
    void projectFolderMenu(int id){
        auto*f=trackFolder(id);if(!f)return;projectTreeFolder=id;HMENU menu=CreatePopupMenu();const wchar_t*names[]={L"New child folder…",L"Rename folder…",L"Move folder into…",L"Dissolve folder, keep tracks",L"Select all tracks in folder",L"Freeze folder and subfolders",L"Unfreeze folder and subfolders"};
        for(int i=0;i<7;i++)AppendMenuW(menu,MF_STRING,i+1,names[i]);POINT pt;GetCursorPos(&pt);int command=TrackPopupMenu(menu,TPM_RETURNCMD,pt.x,pt.y,0,window,nullptr);DestroyMenu(menu);
        try{if(command==6||command==7){freezeFolder(id,command==7);return;}if(command==1){newProjectFolder(id,false);return;}if(command==2){auto name=TextPrompt::show(window,L"Folder name",wide(f->name));if(name&&!name->empty()){history();f->name=utf8(*name);changed();}}
            if(command==3){int parent=chooseProjectFolder(id);if(parent>=-1){auto candidate=project;moveTrackFolder(candidate,id,parent);history();project=std::move(candidate);changed();}}
            if(command==4){auto candidate=project;dissolveTrackFolder(candidate,id);history();project=std::move(candidate);projectTreeFolder=-1;changed();}
            if(command==5){auto group=folderTracks(id);if(!group.empty()){select(*group.begin());mixerSelection=std::move(group);}}
        }catch(const std::exception&e){error(e.what());}refreshProjectRows();InvalidateRect(window,nullptr,FALSE);
    }
    void toggleProjectFolder(int id){auto*f=trackFolder(id);if(!f)return;history();f->collapsed=!f->collapsed;projectTreeFolder=id;refreshProjectRows();changed();}
    void projectTree(Painter&p,float panelWidth,float y){
        refreshProjectRows();button(p,L"+ Folder",{23,y,panelWidth-104,27},ProjectFolderNew,false,-1);button(p,L"Group…",{panelWidth-75,y,63,27},ProjectFolderGroup);y+=35;
        p.text(number(project.tracks.size())+L" / 512 · "+number(project.folders.size())+L" folders",{25,y,panelWidth-39,23},9,muted);y+=28;
        float bottom=height-112;int visible=std::max(1,(int)((bottom-y)/29));projectTreeScroll=(int)clamp(projectTreeScroll,0,std::max(0,(int)projectRows.size()-visible));projectTreeList={21,y,panelWidth-44,std::max(20.f,bottom-y)};
        for(int row=0;row<visible;row++){int pos=projectTreeScroll+row;if(pos>=(int)projectRows.size())break;auto&e=projectRows[pos];Rect r{21,y+row*29,panelWidth-46,27};float indent=std::min(72.f,e.depth*12.f);
            bool active=e.folder?projectTreeFolder==e.id:mixerSelection.count(e.id);if(active)p.fill(r,alpha(crimson,35),4);
            if(e.folder){auto*f=trackFolder(e.id);p.text(f->collapsed?L"▸":L"▾",{r.x+3+indent,r.y,20,r.h},12,crimson,true,1);p.text(wide(f->name),{r.x+26+indent,r.y,r.w-29-indent,r.h},10,textColor,true);addHit(ProjectFolderToggle,r,e.id);}
            else{p.fill({r.x+8+indent,r.y+9,3,10},trackColor(e.id),1);p.text(number(e.id+1)+L" / "+wide(channel(e.id).name),{r.x+20+indent,r.y,r.w-23-indent,r.h},10,active?textColor:muted);addHit(ProjectTreeTrack,r,e.id);}
        }
        navBar(p,{panelWidth-23,y,12,std::max(20.f,bottom-y)},12,projectTreeScroll,std::max(visible,(int)projectRows.size()),visible,true);
        button(p,L"+ Track",{23,height-101,panelWidth-91,26},AddTrack);button(p,L"N…",{panelWidth-60,height-101,48,26},AddTrackCount);
        p.text(L"Ctrl / Shift: select channels",{25,height-68,panelWidth-39,23},9,muted);
    }
    bool projectTreeKey(UINT key){
        if(!leftPanelVisible||!browserVisible||browserTab!=3||navigationFocus!=1)return false;refreshProjectRows();if(projectRows.empty())return false;
        if(key==VK_UP||key==VK_DOWN){projectTreeFocus=(int)clamp(projectTreeFocus+(key==VK_UP?-1:1),0,projectRows.size()-1);auto&e=projectRows[projectTreeFocus];if(e.folder)projectTreeFolder=e.id;else mixerSelect(e.id);if(projectTreeFocus<projectTreeScroll)projectTreeScroll=projectTreeFocus;if(projectTreeFocus>=projectTreeScroll+8)projectTreeScroll=std::max(0,projectTreeFocus-7);InvalidateRect(window,nullptr,FALSE);return true;}
        if(key==VK_LEFT||key==VK_RIGHT||key==VK_RETURN){if(projectTreeFolder>=0){auto*f=trackFolder(projectTreeFolder);if(f&&(key==VK_RETURN||f->collapsed!=(key==VK_LEFT)))toggleProjectFolder(f->id);}return true;}
        return false;
    }
