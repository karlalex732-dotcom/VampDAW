#pragma once
#include <windows.h>
#include <commctrl.h>

namespace vamp {
// Only user-initiated closes restore focus. Background cleanup must never
// activate the application or undo an explicit minimize by the user.
inline void closePanelWindow(HWND panel,bool destroy=false){
    if(!IsWindow(panel))return;
    bool child=(GetWindowLongPtrW(panel,GWL_STYLE)&WS_CHILD)!=0;
    HWND owner=child?GetAncestor(GetParent(panel),GA_ROOT):GetWindow(panel,GW_OWNER);
    if(owner)owner=GetAncestor(owner,GA_ROOT);
    HWND active=GetActiveWindow(),foreground=GetForegroundWindow();
    bool wasActive=active==panel||foreground==panel||
        (child&&active==owner)||(active&&IsChild(panel,active));
    auto capture=GetCapture();if(capture&&(capture==panel||IsChild(panel,capture)))ReleaseCapture();
    if(destroy)DestroyWindow(panel);else ShowWindow(panel,SW_HIDE);
    DWORD process=0;if(owner)GetWindowThreadProcessId(owner,&process);
    if(wasActive&&IsWindow(owner)&&process==GetCurrentProcessId()&&
       IsWindowVisible(owner)&&IsWindowEnabled(owner)&&!IsIconic(owner)){
        SetActiveWindow(owner);SetForegroundWindow(owner);SetFocus(owner);
    }
}
inline LRESULT CALLBACK panelEscapeProc(HWND child,UINT message,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR panel){
    if(message==WM_KEYDOWN&&w==VK_ESCAPE){
        wchar_t kind[32]{};GetClassNameW(child,kind,32);
        if(!wcscmp(kind,L"ComboBox")&&SendMessageW(child,CB_GETDROPPEDSTATE,0,0))return DefSubclassProc(child,message,w,l);
        SendMessageW((HWND)panel,WM_CLOSE,0,0);return 0;
    }
    if(message==WM_NCDESTROY)RemoveWindowSubclass(child,panelEscapeProc,id);
    return DefSubclassProc(child,message,w,l);
}
inline void installPanelEscape(HWND panel){
    EnumChildWindows(panel,[](HWND child,LPARAM owner)->BOOL{SetWindowSubclass(child,panelEscapeProc,0x56414d43,(DWORD_PTR)owner);return TRUE;},(LPARAM)panel);
}
}
