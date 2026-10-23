#pragma once
#include "model.hpp"
#include <map>
#include <cwctype>

namespace vamp::ui {
inline int language=1; // 0 English, 1 Russian. Project data is never changed.
struct Translation{const wchar_t*source;const wchar_t*target;};
inline const Translation english[]={
#include "locale_en.inc"
};
inline const Translation russian[]={
#include "locale_ru.inc"
#include "locale_oracle_ru.inc"
#include "locale_ui048_ru.inc"
#include "locale_ui049_ru.inc"
#include "locale_ui0410_ru.inc"
#include "locale_ui0411_ru.inc"
#include "locale_ui0412_ru.inc"
#include "locale_ui0414_ru.inc"
#include "locale_ui0415_ru.inc"
#include "locale_ui0417_ru.inc"
#include "locale_ui0418_ru.inc"
#include "locale_ui0419_ru.inc"
};
inline bool latin(wchar_t c){return(c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z');}
inline std::wstring translate(const std::wstring&value){
    using Index=std::map<wchar_t,std::vector<const Translation*>>;
    static auto index=[](auto&entries){Index result;for(auto&entry:entries)if(*entry.source)result[*entry.source].push_back(&entry);for(auto&[ch,rows]:result)std::stable_sort(rows.begin(),rows.end(),[](auto*a,auto*b){return wcslen(a->source)>wcslen(b->source);});return result;};
    static auto en=index(english),ru=index(russian);
    auto apply=[](const Index&entries,const std::wstring&text){std::wstring result;result.reserve(text.size()+32);for(size_t i=0;i<text.size();){const Translation*found=nullptr;auto candidates=entries.find(text[i]);if(candidates!=entries.end())for(auto*row:candidates->second){size_t n=wcslen(row->source);if(text.compare(i,n,row->source))continue;if(latin(row->source[0])&&i&&latin(text[i-1]))continue;if(latin(row->source[n-1])&&i+n<text.size()&&latin(text[i+n]))continue;found=row;break;}if(found){result+=found->target;i+=wcslen(found->source);}else result+=text[i++];}return result;};
    auto canonical=apply(en,value);return language?apply(ru,canonical):canonical;
}
inline BOOL appendMenu(HMENU h,UINT flags,UINT_PTR id,LPCWSTR text){if(!text||(flags&MF_BITMAP)||(flags&MF_OWNERDRAW))return ::AppendMenuW(h,flags,id,text);auto value=translate(text);return ::AppendMenuW(h,flags,id,value.c_str());}
inline int messageBox(HWND h,LPCWSTR text,LPCWSTR title,UINT flags){auto a=translate(text?text:L""),b=translate(title?title:L"");return ::MessageBoxW(h,a.c_str(),b.c_str(),flags);}
inline HWND createWindow(DWORD ex,LPCWSTR cls,LPCWSTR title,DWORD style,int x,int y,int w,int h,HWND parent,HMENU menu,HINSTANCE instance,LPVOID param){bool raw=(ULONG_PTR)cls<=65535||!wcscmp(cls,L"EDIT")||!wcscmp(cls,L"COMBOBOX");auto value=raw?std::wstring(title?title:L""):translate(title?title:L"");return ::CreateWindowExW(ex,cls,title?value.c_str():nullptr,style,x,y,w,h,parent,menu,instance,param);}
inline BOOL setText(HWND h,LPCWSTR text){auto value=translate(text?text:L"");return ::SetWindowTextW(h,value.c_str());}
inline BOOL setItemText(HWND h,int id,LPCWSTR text){auto value=translate(text?text:L"");return ::SetDlgItemTextW(h,id,value.c_str());}
inline LRESULT sendRaw(HWND h,UINT msg,WPARAM w,LPARAM l){return ::SendMessageW(h,msg,w,l);}
inline BOOL setItemTextRaw(HWND h,int id,LPCWSTR text){return ::SetDlgItemTextW(h,id,text);}
inline LRESULT send(HWND h,UINT msg,WPARAM w,LPARAM l){if(l&&(msg==CB_ADDSTRING||msg==CB_INSERTSTRING||msg==LB_ADDSTRING)){auto value=translate((LPCWSTR)l);return ::SendMessageW(h,msg,w,(LPARAM)value.c_str());}return ::SendMessageW(h,msg,w,l);}
inline LRESULT sendItem(HWND h,int id,UINT msg,WPARAM w,LPARAM l){if(l&&(msg==CB_ADDSTRING||msg==CB_INSERTSTRING||msg==LB_ADDSTRING)){auto value=translate((LPCWSTR)l);return ::SendDlgItemMessageW(h,id,msg,w,(LPARAM)value.c_str());}return ::SendDlgItemMessageW(h,id,msg,w,l);}
}
