#pragma once
#include "model.hpp"
#include <windowsx.h>
namespace vamp {
inline void scaleNativeWindow(HWND window,double&zoom,int delta,std::vector<HFONT>&fonts){
    double next=clamp(zoom*(delta>0?1.07:1/1.07),.65,1.8),factor=next/zoom;if(std::abs(factor-1)<1e-8)return;struct Context{HWND parent;double factor;std::vector<HFONT>fonts;};Context context{window,factor,{}};
    EnumChildWindows(window,[](HWND child,LPARAM data)->BOOL{auto&c=*(Context*)data;if(GetParent(child)!=c.parent)return TRUE;RECT rect;GetWindowRect(child,&rect);POINT p{rect.left,rect.top};ScreenToClient(c.parent,&p);SetWindowPos(child,nullptr,(int)std::round(p.x*c.factor),(int)std::round(p.y*c.factor),(int)std::round((rect.right-rect.left)*c.factor),(int)std::round((rect.bottom-rect.top)*c.factor),SWP_NOZORDER|SWP_NOACTIVATE);HFONT old=(HFONT)SendMessageW(child,WM_GETFONT,0,0);LOGFONTW info{};if(old&&GetObjectW(old,sizeof info,&info)){info.lfHeight=(LONG)std::round(info.lfHeight*c.factor);HFONT font=CreateFontIndirectW(&info);if(font){SendMessageW(child,WM_SETFONT,(WPARAM)font,TRUE);c.fonts.push_back(font);}}return TRUE;},(LPARAM)&context);
    for(auto font:fonts)DeleteObject(font);fonts=std::move(context.fonts);RECT rect;GetWindowRect(window,&rect);SetWindowPos(window,nullptr,0,0,(int)std::round((rect.right-rect.left)*factor),(int)std::round((rect.bottom-rect.top)*factor),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);zoom=next;InvalidateRect(window,nullptr,TRUE);
}
}
