#pragma once
#include "oracle_chain.hpp"
namespace vamp {
inline std::optional<std::string>chooseOracleEffect(HWND owner,const Json&catalog,const AssistantStep&step){
    auto search=TextPrompt::show(owner,ui::translate(L"Find an effect by name or vendor (empty = all)"),L"");if(!search)return{};auto query=oracleKey(utf8(*search));std::vector<const Json*>entries;for(auto&entry:catalog)if(oracleExternalEffect(entry)){auto text=oracleKey(entry.value("name","")+entry.value("vendor",""));if(query.empty()||text.find(query)!=std::string::npos)entries.push_back(&entry);}std::sort(entries.begin(),entries.end(),[&](auto*a,auto*b){int aa=oracleRoleScore(step.kind,*a),bb=oracleRoleScore(step.kind,*b);return aa==bb?a->value("name","")<b->value("name",""):aa>bb;});
    HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,ui::translate(L"Use the tuned Vamp effect").c_str());AppendMenuW(menu,MF_STRING,2,ui::translate(L"Automatic choice").c_str());AppendMenuW(menu,MF_SEPARATOR,0,nullptr);std::vector<HMENU>pages;
    for(size_t i=0;i<entries.size();i++){HMENU page=menu;if(entries.size()>30){if(i%30==0){page=CreatePopupMenu();pages.push_back(page);auto title=std::to_wstring(i+1)+L"–"+std::to_wstring(std::min(i+30,entries.size()));AppendMenuW(menu,MF_POPUP,(UINT_PTR)page,title.c_str());}page=pages.back();}auto&e=*entries[i];auto label=wide(e.value("name","")+" / "+e.value("vendor",""));AppendMenuW(page,MF_STRING,100+(UINT_PTR)i,label.c_str());}
    if(entries.empty())AppendMenuW(menu,MF_GRAYED,0,ui::translate(L"No matching effects. Scan your plugin folders first.").c_str());POINT p;GetCursorPos(&p);int id=TrackPopupMenu(menu,TPM_RETURNCMD,p.x,p.y,0,owner,nullptr);DestroyMenu(menu);if(id==1)return std::string();if(id==2)return std::string("@automatic");if(id>=100&&id<100+(int)entries.size())return entries[id-100]->value("path","");return{};
}
}
