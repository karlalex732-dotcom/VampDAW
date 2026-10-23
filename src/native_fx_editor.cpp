#include "plugin_brand.hpp"
#include "native_window_style.hpp"
#include "window_lifecycle.hpp"
#include "native_fx.hpp"
#include "localization.hpp"
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <complex>
#include <sstream>
#include <iomanip>
#include "ui_prompt.hpp"
#include "oracle_picker.hpp"
#include "native_biquad.hpp"
#include <gdiplus.h>
#include <dwmapi.h>
namespace vamp {
namespace {
const COLORREF&background=ui::nativePalette.panel,&panelColor=ui::nativePalette.field,&accent=ui::nativePalette.accent,&ink=ui::nativePalette.text,&mutedInk=ui::nativePalette.muted;
std::wstring numberFx(double value,int digits=1){std::wostringstream s;s<<std::fixed<<std::setprecision(digits)<<value;return s.str();}
void fillFx(HDC dc,RECT rect,COLORREF color){auto brush=CreateSolidBrush(color);FillRect(dc,&rect,brush);DeleteObject(brush);}
void textFx(HDC dc,const std::wstring&value,RECT rect,COLORREF color=ink,UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS){SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);auto localized=ui::translate(value);DrawTextW(dc,localized.c_str(),-1,&rect,flags);}
struct LayoutFx {
    int w,h,cols,row,header;double zoom;
    LayoutFx(HWND window,double scale,int kind):zoom(scale){RECT r;GetClientRect(window,&r);w=r.right;h=r.bottom;cols=(int)clamp(w/(240*scale),1,4);row=(int)(64*scale);header=(int)((kind==29?242:kind==31?160:142)*scale);}
    RECT rect(int index,int scroll)const{int cw=(w-32)/cols,c=index%cols,r=index/cols;return{16+c*cw,header+r*row-scroll,16+(c+1)*cw-10,header+(r+1)*row-10-scroll};}
};
fs::path presetFile(HWND owner,bool save){wchar_t file[32768]{};OPENFILENAMEW ofn{sizeof ofn};ofn.hwndOwner=owner;ofn.lpstrFilter=L"Vamp effect preset (*.vampreset)\0*.vampreset\0\0";ofn.lpstrFile=file;ofn.nMaxFile=32768;ofn.lpstrDefExt=L"vampreset";ofn.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);return(save?GetSaveFileNameW(&ofn):GetOpenFileNameW(&ofn))?fs::path(file):fs::path{};}
template<size_t N>void fftFx(std::array<std::complex<double>,N>&a){for(size_t i=1,j=0;i<N;i++){size_t bit=N/2;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}for(size_t length=2;length<=N;length*=2){auto step=std::polar(1.,-6.283185307179586/length);for(size_t i=0;i<N;i+=length){std::complex<double>w=1;for(size_t j=0;j<length/2;j++){auto x=a[i+j],y=w*a[i+j+length/2];a[i+j]=x+y;a[i+j+length/2]=x-y;w*=step;}}}}
}
#include "native_eq_editor.inc"
namespace {
constexpr UINT_PTR fxHoverTimer=0x564748;
void paintFxHover(NativeEffect&fx,HDC dc){
    if(!ui::hoverMotionEnabled||!ui::hoverMotionLevel){fx.hoverGlow.reset();KillTimer(fx.window,fxHoverTimer);return;}
    fx.hoverGlow.advance(GetTickCount64());if(!fx.hoverGlow.moving)KillTimer(fx.window,fxHoverTimer);
    auto r=fx.hoverRect;if(fx.hoverGlow.value>0&&r.right>r.left){Gdiplus::Graphics g(dc);ui::drawHoverGlow(g,Gdiplus::RectF((float)r.left,(float)r.top,(float)(r.right-r.left),(float)(r.bottom-r.top)),fx.hoverGlow.value,ui::hoverMotionLevel,accent);}
}
void fxHoverInput(NativeEffect&fx,UINT message,LPARAM l){
    if(message!=WM_MOUSEMOVE&&message!=WM_MOUSELEAVE&&message!=WM_KILLFOCUS&&message!=WM_SIZE&&message!=WM_VSCROLL&&message!=WM_MOUSEWHEEL)return;
    RECT next{};int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);LayoutFx z(fx.window,fx.zoom,fx.kind);
    if(message==WM_MOUSEMOVE){
        if(fx.kind==46){int gap=8,cell=(z.w-44-gap*3)/4;if(y>=230&&y<266)for(int i=0;i<4;i++){RECT box{22+i*(cell+gap),230,22+i*(cell+gap)+cell,266};if(inEqRect(box,x,y))next=box;}}
        else if(fx.kind==0){EqLayout eq(z.w,z.h);for(int i=0;i<4;i++){auto r=eq.detail(i);if(inEqRect(r,x,y))next=r;}for(int i=0;i<2;i++){auto r=eq.output(i);if(inEqRect(r,x,y))next=r;}for(int i=0;i<7;i++){auto r=eq.tab(i);if(inEqRect(r,x,y))next=r;}}
        else if(y>=z.header&&y<z.h-35){for(size_t i=0;i<fx.spec().parameters.size();i++){auto r=z.rect((int)i,fx.scroll);r.top=std::max<LONG>(r.top,z.header);r.bottom=std::min<LONG>(r.bottom,z.h-35);if(inEqRect(r,x,y))next=r;}}
        if(!next.right&&fx.kind!=46&&y>=20&&y<53&&x>=z.w-255){int left=z.w-255;for(int width:{53,88,100}){RECT r{left,20,left+width-5,53};if(inEqRect(r,x,y))next=r;left+=width;}}
        TRACKMOUSEEVENT track{sizeof track,TME_LEAVE,fx.window,0};TrackMouseEvent(&track);
    }
    bool hovered=next.right>next.left;
    if(hovered&&!EqualRect(&next,&fx.hoverRect)){fx.hoverRect=next;fx.hoverGlow.reset();}
    fx.hoverGlow.aim(hovered,GetTickCount64());
    if(!ui::hoverMotionEnabled||!ui::hoverMotionLevel)fx.hoverGlow.reset();
    if(fx.hoverGlow.moving)SetTimer(fx.window,fxHoverTimer,ui::hoverMotionLevel==3?16:33,nullptr);
    InvalidateRect(fx.window,nullptr,FALSE);
}
}
#include "oracle_editor.inc"
void NativeEffect::paint(){
    if(kind==46){paintAssistant(*this);return;}
    PAINTSTRUCT ps;HDC target=BeginPaint(window,&ps);LayoutFx layout(window,zoom,kind);HDC dc=CreateCompatibleDC(target);auto bitmap=CreateCompatibleBitmap(target,std::max(1,layout.w),std::max(1,layout.h));auto oldBitmap=SelectObject(dc,bitmap);
    fillFx(dc,{0,0,layout.w,layout.h},background);fillFx(dc,{0,0,layout.w,4},accent);
    auto font=CreateFontW((int)(16*zoom),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");auto titleFont=CreateFontW((int)(27*zoom),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");auto oldFont=SelectObject(dc,titleFont);
    textFx(dc,wide(spec().name),{22,14,layout.w-255,56});SelectObject(dc,font);
    textFx(dc,L"KARSEI  /  VAMP PLUGINS",{23,57,layout.w-30,80},mutedInk);
    const wchar_t*buttons[]={activeB?L"B ▾":L"A ▾",L"Copy A/B",L"Preset ▾"};int widths[]={53,88,100};int x=layout.w-255;for(int i=0;i<3;i++){RECT r{x,20,x+widths[i]-5,53};fillFx(dc,r,panelColor);textFx(dc,buttons[i],r,i==0?accent:ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);x+=widths[i];}
    double input=20*std::log10(std::max(1e-7f,inputPeak.load())),output=20*std::log10(std::max(1e-7f,outputPeak.load()));
    textFx(dc,L"IN "+numberFx(input)+L" dBFS   OUT "+numberFx(output)+L" dBFS   GR "+numberFx(reduction.load())+L" dB",{22,87,layout.w-22,110});
    RECT meter{22,115,layout.w-22,121};fillFx(dc,meter,panelColor);meter.right=meter.left+(int)((meter.right-meter.left)*clamp((output+60)/60,0,1));fillFx(dc,meter,accent);
    if(kind==29||kind==31){std::wstring loudness=L"M "+numberFx(momentary.load())+L" LUFS   I "+numberFx(integrated.load())+L" LUFS";if(kind==29)loudness+=L"   RMS "+numberFx(20*std::log10(std::max(1e-7f,rms.load())))+L" dBFS   CORR "+numberFx(correlation.load(),2);textFx(dc,loudness,{22,124,layout.w-22,145},mutedInk);}
    if(kind==29){std::array<std::complex<double>,1024>signal;uint64_t head=scopeHead.load(std::memory_order_acquire);for(int n=0;n<1024;n++)signal[n]=scope[(head+n)%1024].load(std::memory_order_relaxed)*(.5-.5*std::cos(6.283185307179586*n/1023));fftFx(signal);auto pen=CreatePen(PS_SOLID,2,accent);auto oldPen=SelectObject(dc,pen);double range=spec().parameters[0].value(values[0].load());int lastX=22,lastY=225;MoveToEx(dc,lastX,lastY,nullptr);for(int px=0;px<layout.w-44;px++){double hz=20*std::pow(std::min(rate*.49,20000.)/20,px/double(std::max(1,layout.w-44)));int bin=(int)clamp(hz/rate*1024,1,511);double db=20*std::log10(std::max(1e-9,std::abs(signal[bin])/256.));int y=225-(int)(76*clamp((db+range)/range,0,1));LineTo(dc,22+px,y);}SelectObject(dc,oldPen);DeleteObject(pen);}
    if(kind==0)paintEqualizer(*this,dc,layout.w,layout.h);
    int saved=SaveDC(dc);IntersectClipRect(dc,0,layout.header-4,layout.w,layout.h-35);
    for(size_t i=0;kind!=0&&i<spec().parameters.size();i++){auto r=layout.rect((int)i,scroll);if(r.bottom<layout.header||r.top>layout.h-35)continue;fillFx(dc,r,panelColor);auto&s=spec().parameters[i];double n=values[i].load(),value=s.value(n);RECT label{r.left+10,r.top+3,r.right-85,r.top+27},valueRect{r.right-86,r.top+3,r.right-8,r.top+27};textFx(dc,wide(s.name),label);textFx(dc,numberFx(value,s.stepped?0:std::abs(value)>=1000?0:std::abs(value)>=100?1:2)+L" "+wide(s.unit),valueRect,accent,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);RECT rail{r.left+10,r.bottom-14,r.right-10,r.bottom-10};fillFx(dc,rail,RGB(65,47,65));rail.right=rail.left+(int)((rail.right-rail.left)*n);fillFx(dc,rail,accent);}
    RestoreDC(dc,saved);fillFx(dc,{0,layout.h-33,layout.w,layout.h},background);
    textFx(dc,kind==0?L"Drag: frequency / gain · Wheel: Q · Right-click: filter / exact · Ctrl: fine":L"Drag / wheel · Ctrl: fine · Right-click: exact · Double-click: reset",{20,layout.h-31,layout.w-160,layout.h-3},mutedInk);
    if(kind>=11&&kind<=14)textFx(dc,L"Quality: "+std::to_wstring(1<<quality.load())+L"x ▾",{layout.w-152,layout.h-31,layout.w-12,layout.h-3},accent,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    paintFxHover(*this,dc);BitBlt(target,0,0,layout.w,layout.h,dc,0,0,SRCCOPY);SelectObject(dc,oldFont);DeleteObject(font);DeleteObject(titleFont);SelectObject(dc,oldBitmap);DeleteObject(bitmap);DeleteDC(dc);EndPaint(window,&ps);
}
LRESULT CALLBACK NativeEffect::windowProc(HWND h,UINT message,WPARAM w,LPARAM l){
    auto*self=(NativeEffect*)GetWindowLongPtrW(h,GWLP_USERDATA);if(message==WM_NCCREATE){self=(NativeEffect*)((CREATESTRUCTW*)l)->lpCreateParams;self->window=h;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)self);}if(!self)return DefWindowProcW(h,message,w,l);
    if(message==WM_NCDESTROY)ui::unwatchHoverWindow(h);
    fxHoverInput(*self,message,l);
    if(message==WM_TIMER&&w==fxHoverTimer){self->hoverGlow.advance(GetTickCount64());if(!self->hoverGlow.moving||!ui::hoverMotionEnabled||!ui::hoverMotionLevel)KillTimer(h,fxHoverTimer);InvalidateRect(h,nullptr,FALSE);return 0;}
    if(self->kind==46)return assistantWindowProc(*self,h,message,w,l);
    auto refresh=[&]{if(self->kind==0){ShowScrollBar(h,SB_VERT,FALSE);InvalidateRect(h,nullptr,FALSE);return;}LayoutFx z(h,self->zoom,self->kind);SCROLLINFO si{sizeof si,SIF_RANGE|SIF_PAGE|SIF_POS};si.nMin=0;si.nMax=(int)((self->spec().parameters.size()+z.cols-1)/z.cols)*z.row;si.nPage=std::max(1,z.h-z.header-40);self->scroll=(int)clamp(self->scroll,0,std::max(0,si.nMax-(int)si.nPage));si.nPos=self->scroll;SetScrollInfo(h,SB_VERT,&si,TRUE);InvalidateRect(h,nullptr,FALSE);};
    auto hit=[&](int x,int y){if(self->kind==0){RECT r;GetClientRect(h,&r);return eqParameterAt(*self,EqLayout(r.right,r.bottom),x,y);}LayoutFx z(h,self->zoom,self->kind);if(y<z.header||y>z.h-35)return -1;for(size_t i=0;i<self->spec().parameters.size();i++){auto rect=z.rect((int)i,self->scroll);POINT point{x,y};if(PtInRect(&rect,point))return(int)i;}return -1;};
    if(self->kind==0&&equalizerInput(*self,h,message,w,l)){refresh();return 0;}
    switch(message){
        case WM_PAINT:self->paint();return 0;
        case WM_ERASEBKGND:return 1;
        case WM_TIMER:if(IsWindowVisible(h))InvalidateRect(h,nullptr,FALSE);return 0;
        case WM_SIZE:refresh();return 0;
        case WM_GETMINMAXINFO:((MINMAXINFO*)l)->ptMinTrackSize=self->kind==0?POINT{720,520}:POINT{480,360};return 0;
        case WM_CLOSE:closePanelWindow(h);return 0;
        case WM_DESTROY:KillTimer(h,1);self->window=nullptr;return 0;
        case WM_VSCROLL:{SCROLLINFO si{sizeof si,SIF_ALL};GetScrollInfo(h,SB_VERT,&si);int code=LOWORD(w);if(code==SB_THUMBTRACK)self->scroll=si.nTrackPos;else self->scroll+=(code==SB_LINEUP?-30:code==SB_LINEDOWN?30:code==SB_PAGEUP?-(int)si.nPage:code==SB_PAGEDOWN?(int)si.nPage:0);refresh();return 0;}
        case WM_KEYDOWN:if(w==VK_ESCAPE){SendMessageW(h,WM_CLOSE,0,0);return 0;}if(w==VK_UP||w==VK_DOWN||w==VK_PRIOR||w==VK_NEXT){self->scroll+=(w==VK_UP?-35:w==VK_DOWN?35:w==VK_PRIOR?-200:200);refresh();return 0;}break;
        case WM_MOUSEWHEEL:{POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&point);int index=hit(point.x,point.y),delta=GET_WHEEL_DELTA_WPARAM(w);if(index>=0&&(GET_KEYSTATE_WPARAM(w)&MK_CONTROL)){self->set(index,self->values[index].load()+delta/120.*.001,true);refresh();}else{self->scroll-=delta/120*45;refresh();}return 0;}
        case WM_LBUTTONDBLCLK:{int index=hit(GET_X_LPARAM(l),GET_Y_LPARAM(l));if(index>=0){auto&p=self->spec().parameters[index];self->set(index,p.normalized(p.initial),true);refresh();}return 0;}
        case WM_RBUTTONUP:{int index=hit(GET_X_LPARAM(l),GET_Y_LPARAM(l));if(index>=0){auto&p=self->spec().parameters[index];auto value=TextPrompt::show(h,ui::translate(wide(p.name))+L" / "+wide(p.unit),numberFx(p.value(self->values[index].load()),4));if(value)try{size_t end=0;double n=std::stod(*value,&end);if(end!=value->size()||!std::isfinite(n))throw std::runtime_error("Invalid number");self->set(index,p.normalized(n),true);}catch(...){MessageBoxW(h,ui::translate(L"Enter a valid number").c_str(),L"Vamp Plugins",MB_OK);}refresh();}return 0;}
        case WM_LBUTTONDOWN:{int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);LayoutFx z(h,self->zoom,self->kind);
            if(y>=20&&y<=53&&x>=z.w-255){int item=x<z.w-202?0:x<z.w-114?1:2;if(item==0){auto&old=self->activeB?self->bankB:self->bankA;auto&next=self->activeB?self->bankA:self->bankB;for(int i=0;i<ParameterCapacity;i++)old[i]=self->values[i].load();self->activeB=!self->activeB;for(size_t i=0;i<self->spec().parameters.size();i++)self->set((uint32_t)i,next[i],true);}else if(item==1){auto&next=self->activeB?self->bankA:self->bankB;for(int i=0;i<ParameterCapacity;i++)next[i]=self->values[i].load();self->edited=true;}else{HMENU menu=CreatePopupMenu();for(auto row:std::array<std::pair<int,const wchar_t*>,4>{{{1,L"Default"},{2,L"Parallel / 35%"},{3,L"Save preset…"},{4,L"Load preset…"}}})if(row.first!=2||self->kind!=31)AppendMenuW(menu,MF_STRING,row.first,ui::translate(row.second).c_str());POINT pt;GetCursorPos(&pt);int command=TrackPopupMenu(menu,TPM_RETURNCMD,pt.x,pt.y,0,h,nullptr);DestroyMenu(menu);if(command==1||command==2){for(size_t i=0;i<self->spec().parameters.size();i++){auto&p=self->spec().parameters[i];self->set((uint32_t)i,p.normalized(command==2&&self->kind!=31&&i==(self->kind==0?18:self->spec().parameters.size()-2)?35:p.initial),true);}}if(command==3||command==4){auto path=presetFile(h,command==3);if(!path.empty())try{if(command==3){std::ofstream file(path);if(!(file<<self->state().dump(2)))throw std::runtime_error("Cannot save preset");}else{std::ifstream file(path);Json data;file>>data;if(data.value("native","")!=self->spec().id)throw std::runtime_error("This preset belongs to a different effect");self->restore(data);self->edited=true;}}catch(const std::exception&e){MessageBoxW(h,wide(e.what()).c_str(),L"Vamp Plugins",MB_OK);}}}refresh();return 0;}
            if(y>=z.h-33&&x>=z.w-155&&self->kind>=11&&self->kind<=14){self->quality=(self->quality.load()+1)%3;self->edited=true;refresh();return 0;}
            int index=hit(x,y);if(index>=0){self->drag=index;self->dragX=(float)x;self->dragValue=self->values[index].load();SetCapture(h);}return 0;
        }
        case WM_MOUSEMOVE:if(self->drag>=0&&(w&MK_LBUTTON)){self->set(self->drag,self->dragValue+(GET_X_LPARAM(l)-self->dragX)*((GetKeyState(VK_CONTROL)&0x8000)?.0002:.004),true);refresh();}return 0;
        case WM_LBUTTONUP:self->drag=-1;ReleaseCapture();return 0;
        case WM_CAPTURECHANGED:self->drag=-1;return 0;
    }return DefWindowProcW(h,message,w,l);
}
void NativeEffect::editor(HWND parent){if(window){ShowWindow(window,SW_SHOW);SetForegroundWindow(window);return;}static bool registered=false;if(!registered){WNDCLASSW c{};c.style=CS_DBLCLKS;c.lpfnWndProc=windowProc;c.hInstance=pluginUiModule();c.lpszClassName=nativeEffectClass;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&c);registered=true;}auto title=wide(spec().name)+L" / VampDAW";window=CreateWindowExW(0,nativeEffectClass,title.c_str(),WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|(kind==0||kind==46?0:WS_VSCROLL),CW_USEDEFAULT,CW_USEDEFAULT,1000,kind==0?740:kind==46?760:660,parent,nullptr,pluginUiModule(),this);if(window){ui::watchHoverWindow(window);ui::styleNativePanel(window);installPanelEscape(window);BOOL dark=TRUE;DwmSetWindowAttribute(window,20,&dark,sizeof dark);SetTimer(window,1,66,nullptr);ShowWindow(window,SW_SHOW);}}
}
