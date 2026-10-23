#pragma once
#include <windows.h>
#include <algorithm>

namespace vamp {
class WindowPaintBuffer {
    HWND window;PAINTSTRUCT paint{};HDC target;HBITMAP bitmap,old;RECT rect{};
public:
    HDC dc;
    explicit WindowPaintBuffer(HWND h):window(h){target=BeginPaint(h,&paint);GetClientRect(h,&rect);dc=CreateCompatibleDC(target);bitmap=CreateCompatibleBitmap(target,std::max(1L,rect.right),std::max(1L,rect.bottom));old=(HBITMAP)SelectObject(dc,bitmap);}
    ~WindowPaintBuffer(){BitBlt(target,0,0,rect.right,rect.bottom,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);EndPaint(window,&paint);}
};
// Draw a small status region off screen and publish it in one transfer.
// The caller must initialize the whole bitmap before this scope ends.
class WindowPaintRegion {
    HDC target;RECT rect;HBITMAP bitmap=nullptr,old=nullptr;
public:
    HDC dc=nullptr;
    WindowPaintRegion(HDC destination,const RECT&bounds):target(destination),rect(bounds){
        if(!target||rect.right<=rect.left||rect.bottom<=rect.top)return;
        dc=CreateCompatibleDC(target);if(!dc)return;
        bitmap=CreateCompatibleBitmap(target,rect.right-rect.left,rect.bottom-rect.top);
        if(bitmap)old=(HBITMAP)SelectObject(dc,bitmap);
    }
    bool valid()const{return dc&&bitmap&&old&&old!=(HBITMAP)HGDI_ERROR;}
    ~WindowPaintRegion(){
        if(valid()){BitBlt(target,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,dc,0,0,SRCCOPY);SelectObject(dc,old);}
        if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);
    }
    WindowPaintRegion(const WindowPaintRegion&)=delete;
    WindowPaintRegion&operator=(const WindowPaintRegion&)=delete;
};
}
