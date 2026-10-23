    struct DeviceWidget{int id;float x,y,w,h;};std::vector<DeviceWidget>deviceWidgets;
    struct DeviceColumn{float x,w;const wchar_t*label;};
    static const std::array<DeviceColumn,4>&asioColumns(){static const std::array<DeviceColumn,4>columns={{{26,140,L"Sample rate"},{176,117,L"Buffer"},{303,124,L"ASIO input"},{437,124,L"ASIO output"}}};return columns;}
    void layoutDeviceControls(){
        if(!deviceWindow||deviceWidgets.empty())return;RECT client;GetClientRect(deviceWindow,&client);
        deviceZoom=std::max(.25,std::min(client.right/582.,client.bottom/730.));
        auto font=CreateFontW(-(int)std::round(13*deviceZoom),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        for(auto&r:deviceWidgets){auto child=GetDlgItem(deviceWindow,r.id);MoveWindow(child,(int)std::round(r.x*deviceZoom),(int)std::round(r.y*deviceZoom),(int)std::round(r.w*deviceZoom),(int)std::round(r.h*deviceZoom),FALSE);SendMessageW(child,WM_SETFONT,(WPARAM)font,FALSE);}
        for(auto old:deviceFonts)DeleteObject(old);deviceFonts={font};RedrawWindow(deviceWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
    }

    void invalidateDeviceStatus(){if(!deviceWindow||!IsWindowVisible(deviceWindow))return;for(auto y:{516,640}){RECT r{(LONG)std::floor(26*deviceZoom),(LONG)std::floor(y*deviceZoom),(LONG)std::ceil(561*deviceZoom),(LONG)std::ceil((y+24)*deviceZoom)};InvalidateRect(deviceWindow,&r,FALSE);}}
