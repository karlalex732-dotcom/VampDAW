    // Sidechain controls edit the existing signal edge, so graph validation and PDC stay shared.
    int sidechainPage=0,sidechainFocusSource=-1,sidechainFocusSlot=-1;
    struct SidechainLink {int source,slot;};
    std::vector<SidechainLink> sidechainLinks()const{
        std::vector<SidechainLink> result;
        for(int source=0;source<(int)project.tracks.size();source++)
            for(int slot=0;slot<(int)project.tracks[source].sends.size();slot++){
                auto&send=project.tracks[source].sends[slot];
                if(send.sidechain&&(source==selected||send.target==selected))result.push_back({source,slot});
            }
        return result;
    }
    Send* sidechainSend(int source,int slot){
        if(source<0||source>=(int)project.tracks.size()||slot<0||slot>=(int)project.tracks[source].sends.size())return nullptr;
        auto&send=project.tracks[source].sends[slot];return send.sidechain?&send:nullptr;
    }
    std::wstring routeName(int index)const{return index==MasterChannel?L"MASTER":number(index+1)+L" / "+wide(channel(index).name);}
    void addSidechain(){
        if(selected==MasterChannel){notify(L"Select a source track to create a sidechain");return;}
        if(channel(selected).sends.size()>=16){notify(L"Maximum 16 sends per track");return;}
        int destination=chooseDestination(selected);if(destination<0)return;
        auto candidate=project;candidate.tracks[selected].sends.push_back({destination,1,true,false});
        applyRouting(candidate);sidechainPage=0;
    }
    void sidechainContext(int source,int slot){
        auto*send=sidechainSend(source,slot);if(!send)return;
        HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Exact sidechain level…");
        AppendMenuW(menu,MF_STRING,2,send->preFader?L"Switch to POST":L"Switch to PRE");
        AppendMenuW(menu,MF_STRING,3,L"Remove sidechain");POINT pt;GetCursorPos(&pt);
        int id=TrackPopupMenu(menu,TPM_RETURNCMD,pt.x,pt.y,0,window,nullptr);DestroyMenu(menu);
        if(id==1)sidechainExact(source,slot);
        else if(id==2||id==3){auto candidate=project;auto&sends=candidate.tracks[source].sends;
            if(id==2)sends[slot].preFader=!sends[slot].preFader;else sends.erase(sends.begin()+slot);applyRouting(candidate);}
    }
    void sidechainExact(int source,int slot){
        auto*send=sidechainSend(source,slot);if(!send)return;
        auto value=TextPrompt::show(window,L"Sidechain level / 0–150%",number(send->gain*100,1));if(!value)return;
        try{size_t consumed=0;double amount=std::stod(*value,&consumed);
            if(consumed!=value->size()||!std::isfinite(amount)||amount<0||amount>150)throw std::runtime_error("Invalid sidechain level");
            history();send->gain=(float)(amount/100);changed();
        }catch(...){notify(L"Enter a sidechain level from 0 to 150%");}
    }
    void mixerRouting(Painter&p,Rect rect,float stripWidth){
        auto links=sidechainLinks();sidechainPage=(int)clamp(sidechainPage,0,std::max(0,((int)links.size()-1)/3));
        p.fill(rect,panel,6);p.text(L"SIDECHAIN",{rect.x+10,rect.y+2,91,24},10,crimson,true);
        p.text(routeName(selected),{rect.x+107,rect.y+2,std::max(50.f,rect.w-285),24},10,textColor,true);
        if(selected!=MasterChannel)button(p,L"+ SC →",{rect.x+rect.w-162,rect.y+3,76,23},SidechainAdd);
        button(p,L"‹",{rect.x+rect.w-79,rect.y+3,32,23},SidechainPage,false,-1);
        button(p,L"›",{rect.x+rect.w-41,rect.y+3,32,23},SidechainPage,false,1);
        auto stripX=[&](int index)->float{if(index==MasterChannel)return work.x+(mixerPageTracks+.5f)*stripWidth;
            return index>=trackOffset&&index<trackOffset+mixerPageTracks?work.x+(index-trackOffset+.5f)*stripWidth:-1;};
        for(auto link:links){auto&send=project.tracks[link.source].sends[link.slot];float from=stripX(link.source),to=stripX(send.target);
            if(from<0||to<0)continue;float y=rect.y-7;
            p.line(from,y-5,from,y,alpha(crimson,180),1.5f);p.line(from,y,to,y,alpha(crimson,180),1.5f);
            p.line(to,y,to,y-5,crimson,1.5f);p.line(to-3,y-2,to,y-5,crimson,1.5f);p.line(to+3,y-2,to,y-5,crimson,1.5f);
        }
        if(links.empty()){p.text(L"No sidechain links. Select a source and use + SC →",{rect.x+12,rect.y+28,rect.w-24,24},10,muted);return;}
        for(int row=0;row<3;row++){int n=sidechainPage*3+row;if(n>=(int)links.size())break;auto link=links[n];auto&send=project.tracks[link.source].sends[link.slot];
            Rect r{rect.x+8,rect.y+31+row*33,rect.w-16,30};p.fill(r,raised,4);
            p.circle(r.x+10,r.y+15,3,send.gain>0?crimson:muted);
            p.text(routeName(link.source)+L"  →  "+routeName(send.target),{r.x+21,r.y,r.w-187,30},10,textColor,true);
            addHit(SidechainLinkMenu,{r.x+18,r.y,r.w-190,30},link.source,link.slot);
            float cx=r.x+r.w-145,cy=r.y+15,radius=10;Pen track(alpha(muted,70),2.5f),arc(crimson,2.5f);
            p.g.DrawArc(&track,cx-radius,cy-radius,radius*2,radius*2,135.f,270.f);
            float fraction=send.gain/1.5f;p.g.DrawArc(&arc,cx-radius,cy-radius,radius*2,radius*2,135.f,270.f*fraction);
            double angle=(135+270*fraction)*3.141592653589793/180;p.line(cx,cy,cx+(float)cos(angle)*8,cy+(float)sin(angle)*8,textColor,1.5f);
            p.text(number(send.gain*100,0)+L"%",{cx+16,r.y,45,30},10,textColor,true);
            addHit(SidechainGain,{cx-14,r.y,76,30},link.source,link.slot);
            p.text(send.preFader?L"PRE":L"POST",{r.x+r.w-76,r.y,35,30},8,muted,false,1);
            button(p,L"×",{r.x+r.w-32,r.y+4,26,22},SidechainRemove,false,link.source);hits.back().extra=link.slot;
        }
    }
