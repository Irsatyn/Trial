#include "settings_panel.h"
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace {
constexpr int Width=200,Opacity=201,FPS=202,Hold=203,Smooth=204,Range=205;
constexpr int Follow=210,Top=211,Through=212,Pause=213,Defaults=300,Cancel=301,Save=302,Status=310;
constexpr int ClientWidth=540,ClientHeight=588;
}
int SettingsPanel::px(int value) const { return static_cast<int>(std::lround(value*scale_)); }

void SettingsPanel::fonts() {
    if (normal_) DeleteObject(normal_);
    if (title_) DeleteObject(title_);
    if (section_) DeleteObject(section_);
    auto make=[this](int size,int weight) {
        return CreateFontW(-px(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    };
    normal_=make(13,FW_NORMAL); title_=make(23,FW_BOLD); section_=make(15,FW_BOLD);
}
void SettingsPanel::relayout() {
    for (const auto& p : controls_) {
        MoveWindow(p.child,px(p.x),px(p.y),px(p.w),px(p.h),TRUE);
        SendMessageW(p.child,WM_SETFONT,reinterpret_cast<WPARAM>(p.style==1?title_:p.style==2?section_:normal_),TRUE);
    }
}
HWND SettingsPanel::child(const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int w,int h,int font) {
    HWND handle=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),
                                hwnd_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(handle,WM_SETFONT,reinterpret_cast<WPARAM>(font==1?title_:font==2?section_:normal_),TRUE);
    controls_.push_back({handle,x,y,w,h,font}); return handle;
}
void SettingsPanel::build() {
    child(L"STATIC",L"BongoCat 配置",0,0,24,18,490,34,1);
    child(L"STATIC",L"调整即可预览，保存后下次启动自动恢复。",0,0,24,59,490,22);
    child(L"STATIC",L"显示",0,0,24,94,490,24,2);
    struct Slider { int id,y,minimum,maximum,step; const wchar_t* name; };
    const Slider sliders[]={{Width,124,384,1020,24,L"窗口大小"},{Opacity,168,30,100,1,L"不透明度"},
        {FPS,248,30,120,5,L"帧率上限"},{Hold,292,10,200,5,L"按键保持"},
        {Smooth,336,10,200,5,L"鼠标平滑"},{Range,380,0,40,1,L"鼠标移动幅度"}};
    child(L"STATIC",L"动作",0,0,24,210,490,24,2);
    for (const auto& s : sliders) {
        child(L"STATIC",s.name,SS_CENTERIMAGE,0,24,s.y,130,28);
        HWND track=child(TRACKBAR_CLASSW,s.name,TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,s.id,156,s.y-4,278,36);
        SendMessageW(track,TBM_SETRANGE,TRUE,MAKELPARAM(s.minimum,s.maximum));
        SendMessageW(track,TBM_SETLINESIZE,0,s.step); SendMessageW(track,TBM_SETPAGESIZE,0,s.step*4);
        child(L"STATIC",L"",SS_RIGHT|SS_CENTERIMAGE,220+s.id-Width,440,s.y,76,28);
    }
    child(L"BUTTON",L"鼠标跟随光标",BS_AUTOCHECKBOX|WS_TABSTOP,Follow,24,434,230,28);
    child(L"BUTTON",L"窗口置顶",BS_AUTOCHECKBOX|WS_TABSTOP,Top,280,434,230,28);
    child(L"BUTTON",L"鼠标穿透",BS_AUTOCHECKBOX|WS_TABSTOP,Through,24,468,230,28);
    child(L"BUTTON",L"暂停动画",BS_AUTOCHECKBOX|WS_TABSTOP,Pause,280,468,230,28);
    EnableWindow(GetDlgItem(hwnd_,Through),trayAvailable_);
    child(L"STATIC",L"当前设置已载入。",SS_CENTERIMAGE,Status,24,506,492,24);
    child(L"BUTTON",L"恢复默认(&R)",BS_PUSHBUTTON|WS_TABSTOP,Defaults,24,544,120,32);
    child(L"BUTTON",L"取消(&C)",BS_PUSHBUTTON|WS_TABSTOP,Cancel,282,544,100,32);
    child(L"BUTTON",L"保存并关闭(&S)",BS_DEFPUSHBUTTON|WS_TABSTOP,Save,394,544,122,32);
    populate(baseline_);
}
void SettingsPanel::populate(const PanelSettings& s) {
    const int values[]={s.width,s.opacity,s.fps,s.holdMs,s.smoothingMs,s.mouseRange};
    for (int i=0;i<6;++i) SendDlgItemMessageW(hwnd_,Width+i,TBM_SETPOS,TRUE,values[i]);
    const bool checks[]={s.followMouse,s.topmost,s.clickThrough,s.paused};
    for (int i=0;i<4;++i) CheckDlgButton(hwnd_,Follow+i,checks[i]?BST_CHECKED:BST_UNCHECKED);
    updateValues();
}
PanelSettings SettingsPanel::read() const {
    PanelSettings s;
    s.width=static_cast<int>(SendDlgItemMessageW(hwnd_,Width,TBM_GETPOS,0,0));
    s.opacity=static_cast<int>(SendDlgItemMessageW(hwnd_,Opacity,TBM_GETPOS,0,0));
    s.fps=static_cast<int>(SendDlgItemMessageW(hwnd_,FPS,TBM_GETPOS,0,0));
    s.holdMs=static_cast<int>(SendDlgItemMessageW(hwnd_,Hold,TBM_GETPOS,0,0));
    s.smoothingMs=static_cast<int>(SendDlgItemMessageW(hwnd_,Smooth,TBM_GETPOS,0,0));
    s.mouseRange=static_cast<int>(SendDlgItemMessageW(hwnd_,Range,TBM_GETPOS,0,0));
    s.followMouse=IsDlgButtonChecked(hwnd_,Follow)==BST_CHECKED;
    s.topmost=IsDlgButtonChecked(hwnd_,Top)==BST_CHECKED;
    s.clickThrough=trayAvailable_ && IsDlgButtonChecked(hwnd_,Through)==BST_CHECKED;
    s.paused=IsDlgButtonChecked(hwnd_,Pause)==BST_CHECKED;
    s.sanitize(); return s;
}
void SettingsPanel::updateValues() {
    auto s=read(); const int values[]={s.width,s.opacity,s.fps,s.holdMs,s.smoothingMs,s.mouseRange};
    const wchar_t* units[]={L" px",L"%",L" fps",L" ms",L" ms",L" px"};
    for (int i=0;i<6;++i) SetDlgItemTextW(hwnd_,220+i,(std::to_wstring(values[i])+units[i]).c_str());
}
void SettingsPanel::preview(bool deferWidth) {
    updateValues(); auto next=read();
    if (deferWidth) next.width=applied_.width;
    apply_(next,false); applied_=next;
    SetDlgItemTextW(hwnd_,Status,L"正在预览 · 尚未保存");
}
LRESULT CALLBACK SettingsPanel::procedure(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam) {
    auto* self=reinterpret_cast<SettingsPanel*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if (msg==WM_NCCREATE) {
        self=static_cast<SettingsPanel*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        self->hwnd_=hwnd; SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    return self?self->message(msg,wparam,lparam):DefWindowProcW(hwnd,msg,wparam,lparam);
}
LRESULT SettingsPanel::message(UINT msg,WPARAM wparam,LPARAM lparam) {
    switch (msg) {
    case WM_ENTERSIZEMOVE:
    case WM_EXITSIZEMOVE:
    case WM_ENTERMENULOOP:
    case WM_EXITMENULOOP:
        SendMessageW(owner_,msg,wparam,lparam); return 0;
    case WM_CREATE: fonts(); build(); return 0;
    case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(wparam),RGB(45,49,55));
        SetBkColor(reinterpret_cast<HDC>(wparam),RGB(248,249,251));
        return reinterpret_cast<LRESULT>(background_);
    case WM_ERASEBKGND: {
        RECT rect{}; GetClientRect(hwnd_,&rect); FillRect(reinterpret_cast<HDC>(wparam),&rect,background_); return 1;
    }
    case WM_HSCROLL:
        preview(GetDlgCtrlID(reinterpret_cast<HWND>(lparam))==Width && LOWORD(wparam)==TB_THUMBTRACK); return 0;
    case WM_COMMAND:
        if (LOWORD(wparam)>=Follow && LOWORD(wparam)<=Pause && HIWORD(wparam)==BN_CLICKED) { preview(); return 0; }
        switch (LOWORD(wparam)) {
        case Defaults: { PanelSettings s; populate(s); preview(); return 0; }
        case IDOK:
        case Save: { auto next=read(); apply_(next,true); close(false); return 0; }
        case IDCANCEL:
        case Cancel: close(true); return 0;
        }
        break;
    case WM_DPICHANGED: {
        RECT suggested=*reinterpret_cast<const RECT*>(lparam);
        MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
        GetMonitorInfoW(MonitorFromRect(&suggested,MONITOR_DEFAULTTONEAREST),&monitor);
        scale_=std::min({HIWORD(wparam)/96.f,(monitor.rcWork.right-monitor.rcWork.left-40)/540.f,
                          (monitor.rcWork.bottom-monitor.rcWork.top-80)/588.f});
        scale_=std::max(0.6f,scale_); fonts(); relayout();
        RECT outer{0,0,px(ClientWidth),px(ClientHeight)};
        AdjustWindowRectExForDpi(&outer,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,WS_EX_CONTROLPARENT,HIWORD(wparam));
        SetWindowPos(hwnd_,nullptr,suggested.left,suggested.top,outer.right-outer.left,outer.bottom-outer.top,SWP_NOZORDER|SWP_NOACTIVATE);
        return 0;
    }
    case WM_CLOSE: close(true); return 0;
    case WM_NCDESTROY: {
        HWND old=hwnd_; hwnd_=nullptr; controls_.clear();
        return DefWindowProcW(old,msg,wparam,lparam);
    }
    }
    return DefWindowProcW(hwnd_,msg,wparam,lparam);
}
void SettingsPanel::show(HWND owner,const PanelSettings& current,bool trayAvailable,std::function<void(const PanelSettings&,bool)> callback) {
    if (isOpen()) { ShowWindow(hwnd_,SW_RESTORE); SetForegroundWindow(hwnd_); return; }
    baseline_=current; applied_=current; trayAvailable_=trayAvailable; apply_=std::move(callback);
    owner_=owner; GetWindowRect(owner_,&baselineBounds_);
    if (!background_) background_=CreateSolidBrush(RGB(248,249,251));
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES}; InitCommonControlsEx(&controls);
    WNDCLASSW type{}; type.lpfnWndProc=procedure; type.hInstance=GetModuleHandleW(nullptr);
    type.lpszClassName=L"BongoCatSettingsPanel"; type.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    type.hIcon=LoadIconW(type.hInstance,MAKEINTRESOURCEW(100));
    type.hbrBackground=background_; RegisterClassW(&type);
    MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor);
    UINT dpi=GetDpiForWindow(owner); if (!dpi) dpi=96;
    scale_=std::min({dpi/96.f,(monitor.rcWork.right-monitor.rcWork.left-40)/540.f,
                     (monitor.rcWork.bottom-monitor.rcWork.top-80)/588.f});
    scale_=std::max(0.6f,scale_);
    RECT outer{0,0,px(ClientWidth),px(ClientHeight)};
    DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    AdjustWindowRectExForDpi(&outer,style,FALSE,WS_EX_CONTROLPARENT,dpi);
    int w=outer.right-outer.left,h=outer.bottom-outer.top;
    int x=monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-w)/2;
    int y=monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-h)/2;
    // Independent normal window keeps click-through and topmost on the cat from affecting configuration.
    CreateWindowExW(WS_EX_CONTROLPARENT,type.lpszClassName,L"BongoCat 配置",style,x,y,w,h,
                    nullptr,nullptr,type.hInstance,this);
    if (hwnd_) { ShowWindow(hwnd_,SW_SHOW); SetForegroundWindow(hwnd_); SetFocus(GetDlgItem(hwnd_,Width)); }
}
void SettingsPanel::close(bool restore) {
    if (!isOpen()) return;
    if (restore && apply_) {
        apply_(baseline_,false);
        RECT bounds{}; GetWindowRect(owner_,&bounds);
        int w=bounds.right-bounds.left,h=bounds.bottom-bounds.top;
        int x=baselineBounds_.left,y=baselineBounds_.top;
        RECT candidate{x,y,x+w,y+h}; MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
        if (GetMonitorInfoW(MonitorFromRect(&candidate,MONITOR_DEFAULTTONEAREST),&monitor)) {
            x=std::clamp<int>(x,monitor.rcWork.left,std::max(monitor.rcWork.left,monitor.rcWork.right-w));
            y=std::clamp<int>(y,monitor.rcWork.top,std::max(monitor.rcWork.top,monitor.rcWork.bottom-h));
        }
        SetWindowPos(owner_,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
    DestroyWindow(hwnd_);
    UnregisterClassW(L"BongoCatSettingsPanel",GetModuleHandleW(nullptr));
    if (normal_) { DeleteObject(normal_); normal_=nullptr; }
    if (title_) { DeleteObject(title_); title_=nullptr; }
    if (section_) { DeleteObject(section_); section_=nullptr; }
    if (background_) { DeleteObject(background_); background_=nullptr; }
}
SettingsPanel::~SettingsPanel() {
    close(false);
    if (normal_) DeleteObject(normal_);
    if (title_) DeleteObject(title_);
    if (section_) DeleteObject(section_);
    if (background_) DeleteObject(background_);
}
