#pragma once
#include "window_paint.hpp"
#include "hover_motion.hpp"
#include <commctrl.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <string>
#include <vector>

namespace vamp::ui {
struct NativePalette {
    COLORREF panel=RGB(22,21,29),field=RGB(30,27,37),line=RGB(47,41,54),
        text=RGB(234,225,235),muted=RGB(139,131,150),accent=RGB(215,67,98);
};
inline NativePalette nativePalette;
inline bool hoverMotionEnabled=true;
inline int hoverMotionLevel=2;
inline std::vector<HWND> hoverWindows;
inline void watchHoverWindow(HWND h){if(std::find(hoverWindows.begin(),hoverWindows.end(),h)==hoverWindows.end())hoverWindows.push_back(h);}
inline void unwatchHoverWindow(HWND h){hoverWindows.erase(std::remove(hoverWindows.begin(),hoverWindows.end(),h),hoverWindows.end());}
inline void setHoverMotion(bool enabled,int level){hoverMotionEnabled=enabled;hoverMotionLevel=level;for(HWND h:hoverWindows)InvalidateRect(h,nullptr,FALSE);}
inline void drawHoverGlow(Gdiplus::Graphics&g,Gdiplus::RectF rect,float amount,int level,COLORREF accent){
    if(amount<=0||rect.Width<=2||rect.Height<=2)return;
    float power=amount*(level==3?1.f:level==2?.65f:.4f);
    auto state=g.Save();g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    auto path=[&](Gdiplus::GraphicsPath&p,float inset){float x=rect.X+inset,y=rect.Y+inset,w=rect.Width-inset*2,h=rect.Height-inset*2,d=std::min(12.f,std::min(w,h));if(d<=0)return;p.AddArc(x,y,d,d,180,90);p.AddArc(x+w-d,y,d,d,270,90);p.AddArc(x+w-d,y+h-d,d,d,0,90);p.AddArc(x,y+h-d,d,d,90,90);p.CloseFigure();};
    auto color=[&](int alpha){return Gdiplus::Color((BYTE)std::min(255,int(alpha*power)),GetRValue(accent),GetGValue(accent),GetBValue(accent));};
    Gdiplus::GraphicsPath edge;path(edge,.8f);Gdiplus::SolidBrush wash(color(14));g.FillPath(&wash,&edge);
    for(int i=level==3?3:1;i>=1;i--){Gdiplus::Pen halo(color(i==1?34:10),float(i*2+1));g.DrawPath(&halo,&edge);}
    Gdiplus::Pen outline(color(210),1.2f);g.DrawPath(&outline,&edge);g.Restore(state);
}
struct NativeBrush {HBRUSH handle=nullptr;COLORREF color=~COLORREF(0);~NativeBrush(){if(handle)DeleteObject(handle);}HBRUSH get(COLORREF value){if(!handle||color!=value){if(handle)DeleteObject(handle);handle=CreateSolidBrush(color=value);}return handle;}};
inline NativeBrush panelBrush,fieldBrush,accentBrush;
inline HFONT nativeFont(){static HFONT font=CreateFontW(-14,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");return font;}
inline Gdiplus::Color nativeColor(COLORREF c){return Gdiplus::Color(255,GetRValue(c),GetGValue(c),GetBValue(c));}
inline std::wstring nativeText(HWND h){int n=GetWindowTextLengthW(h);std::wstring s(n+1,L'\0');GetWindowTextW(h,s.data(),n+1);s.resize(n);return s;}
inline void nativeLabel(HDC dc,const std::wstring&text,RECT r,COLORREF color,UINT alignment=DT_LEFT){auto font=(HFONT)SendMessageW(WindowFromDC(dc),WM_GETFONT,0,0);auto old=SelectObject(dc,font?font:nativeFont());SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);DrawTextW(dc,text.c_str(),(int)text.size(),&r,alignment|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);SelectObject(dc,old);}
struct NativeControlState {bool hover=false;HoverEnvelope glow;};
inline void drawNativeWidget(HWND h,HDC dc,float glow=0){
    RECT r{};GetClientRect(h,&r);auto&c=nativePalette;wchar_t kind[64]{};GetClassNameW(h,kind,64);bool button=!_wcsicmp(kind,L"BUTTON"),combo=!_wcsicmp(kind,L"COMBOBOX");if(!button&&!combo)return;
    FillRect(dc,&r,panelBrush.get(c.panel));Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    bool enabled=IsWindowEnabled(h),focus=GetFocus()==h;DWORD style=(DWORD)GetWindowLongPtrW(h,GWL_STYLE);int type=style&BS_TYPEMASK;
    bool check=button&&(type==BS_CHECKBOX||type==BS_AUTOCHECKBOX||type==BS_RADIOBUTTON||type==BS_AUTORADIOBUTTON);
    auto rounded=[&](float x,float y,float width,float height,COLORREF color){float d=std::min(10.f,std::min(width,height));if(d<=0)return;Gdiplus::GraphicsPath path;path.AddArc(x,y,d,d,180,90);path.AddArc(x+width-d,y,d,d,270,90);path.AddArc(x+width-d,y+height-d,d,d,0,90);path.AddArc(x,y+height-d,d,d,90,90);path.CloseFigure();Gdiplus::SolidBrush brush(nativeColor(color));g.FillPath(&brush,&path);};
    RECT text=r;auto ink=enabled?c.text:c.muted;
    if(check){bool checked=SendMessageW(h,BM_GETCHECK,0,0)==BST_CHECKED;float y=(r.bottom-16)/2.f;rounded(1,y,16,16,checked?c.accent:c.line);if(checked){Gdiplus::Pen pen(nativeColor(c.panel),2);g.DrawLine(&pen,4.f,y+8,7.f,y+11);g.DrawLine(&pen,7.f,y+11,13.f,y+4);}text.left+=25;}
    else {rounded(0,0,(float)r.right,(float)r.bottom,focus?c.accent:c.line);rounded(1,1,(float)r.right-2,(float)r.bottom-2,c.field);text.left+=8;text.right-=combo?28:8;}
    if(enabled&&hoverMotionEnabled&&hoverMotionLevel>0&&glow>0)drawHoverGlow(g,Gdiplus::RectF(1,1,(float)r.right-2,(float)r.bottom-2),glow,hoverMotionLevel,c.accent);
    auto font=(HFONT)SendMessageW(h,WM_GETFONT,0,0);auto old=SelectObject(dc,font?font:nativeFont());SetBkMode(dc,TRANSPARENT);SetTextColor(dc,ink);auto value=nativeText(h);DrawTextW(dc,value.c_str(),(int)value.size(),&text,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX|(!combo&&!check?DT_CENTER:DT_LEFT));SelectObject(dc,old);
    if(combo){float x=r.right-13.f,y=r.bottom/2.f;Gdiplus::Pen pen(nativeColor(ink),1.4f);g.DrawLine(&pen,x-3,y-2,x,y+1);g.DrawLine(&pen,x,y+1,x+3,y-2);}
    if(focus&&check){Gdiplus::Pen pen(nativeColor(c.accent));g.DrawLine(&pen,25.f,(float)r.bottom-1,(float)r.right,(float)r.bottom-1);}
}
inline LRESULT CALLBACK nativeControlProc(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data){
    auto*s=(NativeControlState*)data;
    constexpr UINT_PTR timer=0x564748;
    auto animate=[&]{if(!hoverMotionEnabled||!hoverMotionLevel||!IsWindowEnabled(h)){s->glow.reset();KillTimer(h,timer);}else{s->glow.aim(s->hover,GetTickCount64());if(s->glow.moving)SetTimer(h,timer,hoverMotionLevel==3?16:33,nullptr);else KillTimer(h,timer);}};
    if(msg==WM_PAINT){animate();WindowPaintBuffer buffer(h);drawNativeWidget(h,buffer.dc,s->glow.value);return 0;}
    if(msg==WM_PRINTCLIENT){animate();drawNativeWidget(h,(HDC)w,s->glow.value);return 0;}
    if(msg==WM_TIMER&&w==timer){animate();InvalidateRect(h,nullptr,FALSE);return 0;}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_MOUSEMOVE&&!s->hover){s->hover=true;TRACKMOUSEEVENT track{sizeof track,TME_LEAVE,h,0};TrackMouseEvent(&track);animate();InvalidateRect(h,nullptr,FALSE);}
    if(msg==WM_MOUSELEAVE){s->hover=false;animate();InvalidateRect(h,nullptr,FALSE);}
    if(msg==WM_SETFOCUS||msg==WM_KILLFOCUS||msg==WM_ENABLE||msg==BM_SETCHECK||msg==CB_SETCURSEL||msg==WM_SETTEXT)InvalidateRect(h,nullptr,FALSE);
    if(msg==WM_NCDESTROY){KillTimer(h,timer);unwatchHoverWindow(h);RemoveWindowSubclass(h,nativeControlProc,id);delete s;}
    return DefSubclassProc(h,msg,w,l);
}
inline LRESULT CALLBACK nativeHeaderProc(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR){
    if(msg==WM_PAINT||msg==WM_PRINTCLIENT){auto draw=[&](HDC dc){RECT r{};GetClientRect(h,&r);FillRect(dc,&r,fieldBrush.get(nativePalette.field));for(int i=0;i<Header_GetItemCount(h);i++){RECT cell{};Header_GetItemRect(h,i,&cell);wchar_t value[256]{};HDITEMW item{};item.mask=HDI_TEXT|HDI_FORMAT;item.pszText=value;item.cchTextMax=256;Header_GetItem(h,i,&item);cell.left+=10;if(item.fmt&HDF_SORTUP)wcscat_s(value,L" ↑");if(item.fmt&HDF_SORTDOWN)wcscat_s(value,L" ↓");nativeLabel(dc,value,cell,nativePalette.muted);}};
        if(msg==WM_PRINTCLIENT)draw((HDC)w);else{WindowPaintBuffer buffer(h);draw(buffer.dc);}return 0;}
    if(msg==WM_ERASEBKGND)return 1;if(msg==WM_NCDESTROY)RemoveWindowSubclass(h,nativeHeaderProc,id);return DefSubclassProc(h,msg,w,l);
}
inline LRESULT CALLBACK nativePanelProc(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR){
    auto&c=nativePalette;
    if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLORBTN||msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORLISTBOX){bool field=msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORLISTBOX;SetBkColor((HDC)w,field?c.field:c.panel);SetTextColor((HDC)w,c.text);return (LRESULT)(field?fieldBrush.get(c.field):panelBrush.get(c.panel));}
    if(msg==WM_ERASEBKGND){RECT r{};GetClientRect(h,&r);FillRect((HDC)w,&r,panelBrush.get(c.panel));return 1;}
    if(msg==WM_NOTIFY&&l){auto*n=(NMHDR*)l;if(n->code==NM_CUSTOMDRAW){wchar_t kind[64]{};GetClassNameW(n->hwndFrom,kind,64);if(!_wcsicmp(kind,TRACKBAR_CLASSW)){auto*d=(NMCUSTOMDRAW*)l;if(d->dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;if(d->dwDrawStage==CDDS_ITEMPREPAINT){FillRect(d->hdc,&d->rc,d->dwItemSpec==TBCD_THUMB?accentBrush.get(c.accent):d->dwItemSpec==TBCD_CHANNEL?fieldBrush.get(c.line):panelBrush.get(c.panel));return CDRF_SKIPDEFAULT;}}}}
    if(msg==WM_NCDESTROY)RemoveWindowSubclass(h,nativePanelProc,id);return DefSubclassProc(h,msg,w,l);
}
inline void styleNativeControl(HWND h){
    wchar_t kind[64]{};GetClassNameW(h,kind,64);DWORD_PTR data=0;
    if(!_wcsicmp(kind,L"BUTTON")||!_wcsicmp(kind,L"COMBOBOX")){
        if(!_wcsicmp(kind,L"BUTTON")&&(GetWindowLongPtrW(h,GWL_STYLE)&BS_TYPEMASK)==BS_OWNERDRAW)return;
        if(!GetWindowSubclass(h,nativeControlProc,0x56414d53,&data)){auto*s=new NativeControlState();if(!SetWindowSubclass(h,nativeControlProc,0x56414d53,(DWORD_PTR)s))delete s;else watchHoverWindow(h);}
    }
    if(!_wcsicmp(kind,WC_LISTVIEWW)){ListView_SetBkColor(h,nativePalette.panel);ListView_SetTextBkColor(h,nativePalette.panel);ListView_SetTextColor(h,nativePalette.text);auto header=ListView_GetHeader(h);if(header)SetWindowSubclass(header,nativeHeaderProc,0x56414d48,0);}
    auto font=(HFONT)SendMessageW(h,WM_GETFONT,0,0);if(!font||font==GetStockObject(DEFAULT_GUI_FONT))SendMessageW(h,WM_SETFONT,(WPARAM)nativeFont(),FALSE);
}
inline void styleNativePanel(HWND h){
    if(!h)return;SetWindowSubclass(h,nativePanelProc,0x56414d50,0);BOOL dark=TRUE;DwmSetWindowAttribute(h,20,&dark,sizeof dark);DwmSetWindowAttribute(h,35,&nativePalette.panel,sizeof(COLORREF));DwmSetWindowAttribute(h,36,&nativePalette.text,sizeof(COLORREF));
    EnumChildWindows(h,[](HWND child,LPARAM)->BOOL{styleNativeControl(child);return TRUE;},0);RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
}
