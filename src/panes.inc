    void showEditor(int pane,bool toggle=false){
        if(editGesture)mouseUp();
        if(toggle&&editorVisible&&mode==pane){closePane(pane);return;}
        if(maximizedPane>=0&&maximizedPane!=pane){maximizedPane=-1;maximizeRestoreMode=-1;}
        mode=pane;editorVisible=true;navigationFocus=0;InvalidateRect(window,nullptr,FALSE);
    }
    void closePane(int pane){
        if(editGesture)mouseUp();
        if((pane>=0&&pane<5)||pane==7||pane==9)editorVisible=false;
        if(pane==5)leftPanelVisible=false;
        if(pane==6)inspectorVisible=false;
        if(pane==7)automationVisible=false;
        if(pane==8)mixerDockVisible=false;
        if(maximizedPane==pane){maximizedPane=-1;if(maximizeRestoreMode>=0){mode=maximizeRestoreMode;editorVisible=true;}maximizeRestoreMode=-1;}
        navigationFocus=0;SetFocus(window);InvalidateRect(window,nullptr,FALSE);
    }
    void closeCurrentPane(){closePane(maximizedPane>=0?maximizedPane:navigationFocus==1?5:navigationFocus==2?6:navigationFocus==3?7:navigationFocus==4?8:mode);}
    void closeAllPanes(){
        if(editGesture)mouseUp();
        editorVisible=leftPanelVisible=inspectorVisible=automationVisible=mixerDockVisible=false;
        maximizedPane=maximizeRestoreMode=-1;navigationFocus=0;settingsOpen=false;exportSettingsOpen=false;openMenu=-1;
        for(auto&chain:plugins)for(auto&plugin:chain)plugin->hideEditor();
        if(themeWindow)closeThemeEditor(false);
        stemLabTab=false;if(stemWindow)ShowWindow(stemWindow,SW_HIDE);
        if(pluginPickerWindow)ShowWindow(pluginPickerWindow,SW_HIDE);
        if(poolWindow)ShowWindow(poolWindow,SW_HIDE);
        if(deviceWindow)ShowWindow(deviceWindow,SW_HIDE);
        if(aggregateWindow)ShowWindow(aggregateWindow,SW_HIDE);
        controllerLearning=false;controllerLearnTarget=-1;if(controllerWindow)ShowWindow(controllerWindow,SW_HIDE);
        if(IsWindowVisible(window)&&!IsIconic(window)){SetActiveWindow(window);SetForegroundWindow(window);SetFocus(window);}InvalidateRect(window,nullptr,FALSE);
    }
    void restorePanels(){editorVisible=leftPanelVisible=inspectorVisible=mixerDockVisible=true;automationVisible=false;maximizedPane=maximizeRestoreMode=-1;navigationFocus=0;InvalidateRect(window,nullptr,FALSE);}
    void emptyWorkspace(Painter&p){
        p.text(L"YOUR WORKSPACE",{work.x+20,work.y+50,work.w-40,40},23,textColor,true,1);
        p.text(L"Open a panel from Views or use a shortcut",{work.x+20,work.y+100,work.w-40,28},12,muted,false,1);
        const wchar_t*names[]={L"Playlist · F5",L"Sequencer · F6",L"Piano roll · F7",L"Mixer · F9",L"Sample Lab · Ctrl+E",L"Automation · Alt+O"};
        const int commands[]={40,42,41,43,44,85};float x=work.x+(work.w-310)/2;
        for(int i=0;i<6;i++)button(p,names[i],{x,work.y+145+i*41.f,310,34},MenuCommand,false,commands[i]);
        button(p,L"Restore panels · Ctrl+Shift+H",{x,work.y+405,310,34},MenuCommand,false,89);
    }

    void renameActive(){
        if(mode==0&&editingClip()){renameClip();return;}if(selected==MasterChannel)return;
        auto&t=channel(selected);auto&name=mode==3?t.name:t.activePattern?t.patterns[t.activePattern-1].name:t.patternName;
        auto value=TextPrompt::show(window,mode==3?L"Название дорожки":L"Название паттерна",wide(name));if(value&&!value->empty()){history();name=utf8(*value);changed();}
    }
    void choosePattern(int delta,int absolute=-1){if(selected==MasterChannel)return;finishMidiRecording();auto&t=channel(selected);int next=(int)clamp(absolute>=0?absolute:t.activePattern+delta,0,t.patterns.size());if(next==t.activePattern)return;history();t.activePattern=next;noteSelection.clear();selectedNote=-1;changed();}
    void muteSelection(bool mute){
        if(!editorVisible||selected==MasterChannel||frozenSource())return;
        if(mode==1&&!noteSelection.empty()){history();auto&notes=editNotes(channel(selected));for(int i:noteSelection)if(i<(int)notes.size())notes[i].muted=mute;changed();}
        if(mode==0&&!clipSelection.empty()){for(auto i:clipSelection)if(channel(i.first).frozen)return;history();for(auto i:clipSelection)project.tracks[i.first].clips[i.second].muted=mute;changed();}
    }
