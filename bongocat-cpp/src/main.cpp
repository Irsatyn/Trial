#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <psapi.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include "renderer.h"
#include "settings_panel.h"

namespace fs=std::filesystem;
constexpr UINT TrayMessage=WM_APP+2;
constexpr UINT Exit=100,Pause=101,Through=102,Small=103,Medium=104,Large=105,Reset=106,Reload=107,Configure=108;
HWND window=nullptr;
std::unique_ptr<Renderer> renderer;
InputState input;
Scene scene;
SettingsPanel panel;
NOTIFYICONDATAW tray{};
fs::path root,settings;
bool clickThrough=false,trayReady=false,frameTimer=false,cursorDirty=true,failed=false,modal=false;
HANDLE frameSignal=nullptr;
LARGE_INTEGER clockFrequency{},lastPresented{};
bool smoke=false,selfTest=false,benchmark=false,idleBenchmark=false;
bool configTrace=false;
int fps=60,tauMs=60,smokeStep=0,width=612,opacity=100,mouseRange=22;
bool followMouse=true,topmost=true;
bool dragArmed=false;
POINT dragOrigin{};
float targetX=0,targetY=0;
std::uint64_t lastFrame=0,rawEvents=0;
UINT taskbarCreated=0;
std::uint64_t configTime=0;
bool profiling() { return smoke || selfTest || benchmark || idleBenchmark; }

void note(const char* text) {
    if (profiling() || configTrace) { std::ofstream log(root/L"smoke-test.log",std::ios::app); log<<text<<'\n'; }
}
void requestFrame() {
    if (!frameTimer) {
        if (frameSignal && !modal) {
            LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
            double wait=std::max(0.0001,1./fps-(now.QuadPart-lastPresented.QuadPart)/static_cast<double>(clockFrequency.QuadPart));
            LARGE_INTEGER due{}; due.QuadPart=-static_cast<LONGLONG>(wait*10000000);
            frameTimer=SetWaitableTimer(frameSignal,&due,0,nullptr,nullptr,FALSE)!=FALSE;
        } else frameTimer=SetTimer(window,1,std::max(8,1000/fps),nullptr)!=0;
        if (!frameTimer) { failed=true; PostMessageW(window,WM_CLOSE,0,0); }
    }
}
void transition(unsigned vk,bool down) {
    if (vk>=256) return;
    bool before=input.down[vk];
    input.key(vk,down,GetTickCount64());
    if (before!=input.down[vk] && !input.paused) requestFrame();
}
std::uint64_t configStamp() {
    // Preserve sub-second FILETIME precision; this MinGW filesystem build rounded to seconds.
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(settings.c_str(),GetFileExInfoStandard,&data)) return 0;
    return (static_cast<std::uint64_t>(data.ftLastWriteTime.dwHighDateTime)<<32)|data.ftLastWriteTime.dwLowDateTime;
}
void rememberConfigTime() {
    configTime=configStamp();
    if (configTrace) std::ofstream(root/L"smoke-test.log",std::ios::app)<<"remember="<<configTime<<'\n';
}
void saveSettings(bool force=false) {
    if (profiling() || (panel.isOpen() && !force)) return;
    RECT bounds{}; GetWindowRect(window,&bounds);
    auto write=[](const wchar_t* section,const wchar_t* name,int value) {
        WritePrivateProfileStringW(section,name,std::to_wstring(value).c_str(),settings.c_str());
    };
    write(L"Window",L"Width",width); write(L"Window",L"X",bounds.left); write(L"Window",L"Y",bounds.top);
    write(L"Window",L"ClickThrough",clickThrough); write(L"Window",L"Paused",input.paused);
    write(L"Window",L"Opacity",opacity); write(L"Window",L"Topmost",topmost);
    write(L"Animation",L"MaxFPS",fps); write(L"Animation",L"MinPressMs",input.holdMs); write(L"Animation",L"SmoothingMs",tauMs);
    write(L"Animation",L"MouseRange",mouseRange); write(L"Animation",L"FollowMouse",followMouse);
    rememberConfigTime();
}
void clampPosition(int& x,int& y,int w,int h) {
    RECT candidate{x,y,x+w,y+h}; MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
    if (GetMonitorInfoW(MonitorFromRect(&candidate,MONITOR_DEFAULTTONEAREST),&monitor)) {
        x=std::clamp<int>(x,monitor.rcWork.left,std::max(monitor.rcWork.left,monitor.rcWork.right-w));
        y=std::clamp<int>(y,monitor.rcWork.top,std::max(monitor.rcWork.top,monitor.rcWork.bottom-h));
    }
}
void applyStyle() {
    if (!trayReady) clickThrough=false;
    SetWindowLongPtrW(window,GWL_EXSTYLE,WS_EX_LAYERED|(topmost?WS_EX_TOPMOST:0)|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|
                      (clickThrough?WS_EX_TRANSPARENT:0));
    SetWindowPos(window,topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
}
bool resizeWindow(int next) {
    RECT bounds{}; GetWindowRect(window,&bounds);
    width=next;
    if (!renderer->resize(width)) return false;
    int x=bounds.left,y=bounds.top;
    clampPosition(x,y,renderer->width(),renderer->height());
    SetWindowPos(window,topmost?HWND_TOPMOST:HWND_NOTOPMOST,x,y,renderer->width(),renderer->height(),SWP_NOACTIVATE);
    requestFrame(); return true;
}
void loadSettings(bool initial) {
    auto read=[](const wchar_t* section,const wchar_t* key,int value) {
        return static_cast<int>(GetPrivateProfileIntW(section,key,value,settings.c_str()));
    };
    int next=std::clamp(read(L"Window",L"Width",width),384,1020);
    if (configTrace) std::ofstream(root/L"smoke-test.log",std::ios::app)<<"load initial="<<initial<<" width="<<next<<'\n';
    fps=std::clamp(read(L"Animation",L"MaxFPS",60),30,120);
    input.holdMs=static_cast<unsigned>(std::clamp(read(L"Animation",L"MinPressMs",50),10,200));
    tauMs=std::clamp(read(L"Animation",L"SmoothingMs",60),10,200);
    opacity=std::clamp(read(L"Window",L"Opacity",100),30,100);
    topmost=read(L"Window",L"Topmost",1)!=0;
    mouseRange=std::clamp(read(L"Animation",L"MouseRange",22),0,40);
    followMouse=read(L"Animation",L"FollowMouse",1)!=0;
    input.paused=read(L"Window",L"Paused",0)!=0;
    clickThrough=read(L"Window",L"ClickThrough",0)!=0;
    if (initial) width=next;
    else {
        if (next!=width && !resizeWindow(next)) { failed=true; PostMessageW(window,WM_CLOSE,0,0); }
        RECT bounds{}; GetWindowRect(window,&bounds);
        int x=read(L"Window",L"X",bounds.left), y=read(L"Window",L"Y",bounds.top);
        clampPosition(x,y,renderer->width(),renderer->height());
        SetWindowPos(window,topmost?HWND_TOPMOST:HWND_NOTOPMOST,x,y,0,0,SWP_NOSIZE|SWP_NOACTIVATE);
        renderer->setOpacity(opacity);
        applyStyle(); cursorDirty=true;
        if (frameTimer) { KillTimer(window,1); if (frameSignal) CancelWaitableTimer(frameSignal); frameTimer=false; }
        requestFrame();
    }
    rememberConfigTime();
}

PanelSettings currentSettings() {
    PanelSettings s;
    s.width=width; s.opacity=opacity; s.fps=fps; s.holdMs=input.holdMs; s.smoothingMs=tauMs;
    s.mouseRange=mouseRange; s.followMouse=followMouse; s.topmost=topmost;
    s.clickThrough=clickThrough; s.paused=input.paused; return s;
}
void applyPanelSettings(const PanelSettings& proposed,bool persist) {
    auto s=proposed; s.sanitize();
    if (input.paused!=s.paused) input.clear();
    opacity=s.opacity; fps=s.fps; input.holdMs=s.holdMs; tauMs=s.smoothingMs;
    mouseRange=s.mouseRange; followMouse=s.followMouse; topmost=s.topmost;
    clickThrough=s.clickThrough; input.paused=s.paused;
    if (s.width!=width && !resizeWindow(s.width)) { failed=true; PostMessageW(window,WM_CLOSE,0,0); return; }
    renderer->setOpacity(opacity); applyStyle(); cursorDirty=true;
    if (frameTimer) { KillTimer(window,1); if (frameSignal) CancelWaitableTimer(frameSignal); frameTimer=false; }
    requestFrame();
    if (persist) saveSettings(true);
}
void openSettings() { panel.show(window,currentSettings(),trayReady,applyPanelSettings); }

void menu() {
    HMENU popup=CreatePopupMenu();
    UINT locked=panel.isOpen()?MF_GRAYED:0;
    AppendMenuW(popup,MF_STRING,Configure,L"图形化配置…");
    AppendMenuW(popup,MF_SEPARATOR,0,nullptr);
    AppendMenuW(popup,MF_STRING|locked|(input.paused?MF_CHECKED:0),Pause,L"暂停动画");
    AppendMenuW(popup,MF_STRING|locked|(clickThrough?MF_CHECKED:0)|(!trayReady?MF_GRAYED:0),Through,L"鼠标穿透");
    AppendMenuW(popup,MF_SEPARATOR,0,nullptr);
    AppendMenuW(popup,MF_STRING|locked|(width==408?MF_CHECKED:0),Small,L"小 · 408 px");
    AppendMenuW(popup,MF_STRING|locked|(width==612?MF_CHECKED:0),Medium,L"中 · 612 px");
    AppendMenuW(popup,MF_STRING|locked|(width==816?MF_CHECKED:0),Large,L"大 · 816 px");
    AppendMenuW(popup,MF_STRING|locked,Reset,L"恢复右下角位置");
    AppendMenuW(popup,MF_STRING|locked,Reload,L"重新加载设置");
    AppendMenuW(popup,MF_SEPARATOR,0,nullptr);
    AppendMenuW(popup,MF_STRING,Exit,L"退出");
    POINT point{}; GetCursorPos(&point); SetForegroundWindow(window);
    UINT choice=TrackPopupMenu(popup,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window,nullptr);
    DestroyMenu(popup); PostMessageW(window,WM_NULL,0,0);
    if (choice) SendMessageW(window,WM_COMMAND,choice,0);
}

void rawInput(HRAWINPUT handle) {
    alignas(RAWINPUT) BYTE bytes[sizeof(RAWINPUT)+64]{};
    UINT count=sizeof(bytes);
    if (GetRawInputData(handle,RID_INPUT,bytes,&count,sizeof(RAWINPUTHEADER))==static_cast<UINT>(-1)) return;
    const auto* raw=reinterpret_cast<const RAWINPUT*>(bytes);
    ++rawEvents;
    if (raw->header.dwType==RIM_TYPEKEYBOARD) {
        const auto& k=raw->data.keyboard;
        unsigned vk=k.VKey;
        if (vk==255 || k.MakeCode==KEYBOARD_OVERRUN_MAKE_CODE) return;
        if (vk==VK_SHIFT) vk=MapVirtualKeyW(k.MakeCode,MAPVK_VSC_TO_VK_EX);
        if (vk==VK_CONTROL) vk=(k.Flags&RI_KEY_E0)?VK_RCONTROL:VK_LCONTROL;
        if (vk==VK_MENU) vk=(k.Flags&RI_KEY_E0)?VK_RMENU:VK_LMENU;
        if (vk==VK_RETURN && (k.Flags&RI_KEY_E0)) return;
        // Non-extended numpad navigation must not illuminate the separate arrow cluster.
        if (!(k.Flags&RI_KEY_E0) && (vk==VK_LEFT || vk==VK_RIGHT || vk==VK_UP || vk==VK_DOWN || vk==VK_DELETE)) return;
        transition(vk,(k.Flags&RI_KEY_BREAK)==0);
    } else if (raw->header.dwType==RIM_TYPEMOUSE) {
        const auto& m=raw->data.mouse;
        for (const auto& b : {std::pair<USHORT,unsigned>{RI_MOUSE_LEFT_BUTTON_DOWN,VK_LBUTTON},
                               {RI_MOUSE_RIGHT_BUTTON_DOWN,VK_RBUTTON},{RI_MOUSE_MIDDLE_BUTTON_DOWN,VK_MBUTTON}}) {
            if (m.usButtonFlags&b.first) transition(b.second,true);
            if (m.usButtonFlags&(b.first<<1)) transition(b.second,false);
        }
        if (m.usButtonFlags&RI_MOUSE_WHEEL) {
            scene.wheel=static_cast<SHORT>(m.usButtonData)>0?1:-1; scene.wheelUntil=GetTickCount64()+100;
            if (!input.paused) requestFrame();
        }
        if ((m.lLastX || m.lLastY) && !input.paused && !benchmark && followMouse && mouseRange>0) { cursorDirty=true; requestFrame(); }
    }
}

void sampleCursor() {
    POINT point{}; if (!GetCursorPos(&point)) return;
    int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w=std::max(1,GetSystemMetrics(SM_CXVIRTUALSCREEN)),h=std::max(1,GetSystemMetrics(SM_CYVIRTUALSCREEN));
    targetX=followMouse?std::clamp((point.x-x)/static_cast<float>(w)*2-1,-1.f,1.f)*mouseRange:0;
    targetY=followMouse?std::clamp((point.y-y)/static_cast<float>(h)*2-1,-1.f,1.f)*mouseRange*0.6f:0;
}
void drawFrame() {
    auto now=GetTickCount64();
    QueryPerformanceCounter(&lastPresented);
    if (benchmark) {
        auto frame=renderer->frames;
        targetX=std::sin(frame*0.045f)*20; targetY=std::cos(frame*0.04f)*12;
        if (frame%8==0) {
            input.clear(); input.key(frame%16?'A':'D',true,now);
            input.key(frame%24?'J':'K',true,now); input.key(VK_LBUTTON,true,now);
        }
    }
    if (cursorDirty && !benchmark) { sampleCursor(); cursorDirty=false; }
    float dt=lastFrame?std::clamp((now-lastFrame)/1000.f,0.001f,0.1f):0.016f;
    lastFrame=now;
    float a=1-std::exp(-dt/(tauMs/1000.f));
    if (input.paused) { scene.mouseX=0; scene.mouseY=0; scene.wheelUntil=0; }
    else { scene.mouseX+=(targetX-scene.mouseX)*a; scene.mouseY+=(targetY-scene.mouseY)*a; }
    bool moving=!input.paused && (std::abs(targetX-scene.mouseX)>0.08f || std::abs(targetY-scene.mouseY)>0.08f);
    if (!moving && !input.paused) { scene.mouseX=targetX; scene.mouseY=targetY; }
    input.presented(now);
    if (!renderer->draw(input,scene,now)) { failed=true; note("FAIL: render"); DestroyWindow(window); return; }
    if (moving || input.pending(now) || now<scene.wheelUntil || benchmark) requestFrame();
    else lastFrame=0;
}

LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
    if (taskbarCreated && message==taskbarCreated) {
        trayReady=Shell_NotifyIconW(NIM_ADD,&tray)!=FALSE; applyStyle(); return 0;
    }
    switch (message) {
    case WM_INPUT:
        rawInput(reinterpret_cast<HRAWINPUT>(lparam));
        return DefWindowProcW(hwnd,message,wparam,lparam);
    case WM_INPUT_DEVICE_CHANGE: input.clear(); requestFrame(); return 0;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_LBUTTONDBLCLK:
        dragArmed=false; if (GetCapture()==hwnd) ReleaseCapture(); openSettings(); return 0;
    case WM_LBUTTONDOWN:
        GetCursorPos(&dragOrigin); dragArmed=true; SetCapture(hwnd); return 0;
    case WM_MOUSEMOVE:
        if (dragArmed) {
            POINT cursor{}; GetCursorPos(&cursor);
            if (std::abs(cursor.x-dragOrigin.x)>GetSystemMetrics(SM_CXDRAG) ||
                std::abs(cursor.y-dragOrigin.y)>GetSystemMetrics(SM_CYDRAG)) {
                dragArmed=false; ReleaseCapture(); SendMessageW(hwnd,WM_NCLBUTTONDOWN,HTCAPTION,0);
            }
        }
        return 0;
    case WM_LBUTTONUP:
        dragArmed=false; if (GetCapture()==hwnd) ReleaseCapture(); return 0;
    case WM_CAPTURECHANGED: dragArmed=false; return 0;
    case WM_ENTERMENULOOP:
    case WM_ENTERSIZEMOVE:
        modal=true;
        if (frameSignal) CancelWaitableTimer(frameSignal);
        KillTimer(hwnd,1); frameTimer=false; requestFrame(); return 0;
    case WM_EXITMENULOOP:
    case WM_EXITSIZEMOVE:
        modal=false;
        KillTimer(hwnd,1); frameTimer=false; requestFrame();
        if (message==WM_EXITSIZEMOVE) saveSettings();
        return 0;
    case WM_RBUTTONUP: menu(); return 0;
    case TrayMessage:
        if (lparam==WM_RBUTTONUP || lparam==WM_LBUTTONUP) menu();
        return 0;
    case WM_TIMER:
        if (wparam==1) { KillTimer(window,1); frameTimer=false; drawFrame(); }
        else if (wparam==2) {
            // Only check held keys for releases; no 256-key poll and no idle redraw.
            if (!benchmark && !smoke) for (unsigned vk=0;vk<256;++vk)
                if (input.down[vk] && !(GetAsyncKeyState(vk)&0x8000)) transition(vk,false);
            if (!profiling() && !panel.isOpen()) {
                auto time=configStamp();
                if (configTrace) std::ofstream(root/L"smoke-test.log",std::ios::app)<<"poll="<<time<<" current="<<configTime<<'\n';
                if (time && time!=configTime) loadSettings(false);
            }
        } else if (wparam==3) {
            ++smokeStep;
            if (smoke) {
                switch (smokeStep) {
                case 2: transition('A',true); transition('J',true); transition(VK_LBUTTON,true); break;
                case 4: renderer->save(root/L"preview-pressed.png"); break;
                case 6: transition('A',false); transition('J',false); transition(VK_LBUTTON,false); break;
                case 8: SendMessageW(hwnd,WM_ENTERSIZEMOVE,0,0); break;
                case 9: transition('W',true); transition('W',false); break;
                case 10: SendMessageW(hwnd,WM_COMMAND,Small,0); break;
                case 11: SendMessageW(hwnd,WM_EXITSIZEMOVE,0,0); break;
                case 12: SendMessageW(hwnd,WM_COMMAND,Large,0); break;
                case 14: SendMessageW(hwnd,WM_COMMAND,Medium,0); break;
                case 16: SendMessageW(hwnd,WM_COMMAND,Through,0); break;
                case 18: SendMessageW(hwnd,WM_COMMAND,Through,0); break;
                case 20: SendMessageW(hwnd,WM_COMMAND,Pause,0); break;
                case 22: SendMessageW(hwnd,WM_COMMAND,Pause,0); break;
                case 26: renderer->save(root/L"preview.png"); break;
                case 28: note("PASS: raw-input registration, mapped chord, mouse, cached render, resize, pause, click-through"); DestroyWindow(hwnd); break;
                }
            }
        } else if (wparam==4) DestroyWindow(hwnd);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case Configure: openSettings(); return 0;
        case Exit: panel.close(true); DestroyWindow(hwnd); return 0;
        case Pause: input.paused=!input.paused; input.clear(); cursorDirty=true; break;
        case Through: if (trayReady) { clickThrough=!clickThrough; applyStyle(); } break;
        case Small: if (!resizeWindow(408)) failed=true; break;
        case Medium: if (!resizeWindow(612)) failed=true; break;
        case Large: if (!resizeWindow(816)) failed=true; break;
        case Reset: {
            RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
            SetWindowPos(hwnd,topmost?HWND_TOPMOST:HWND_NOTOPMOST,work.right-renderer->width()-24,work.bottom-renderer->height()-24,
                         0,0,SWP_NOSIZE|SWP_NOACTIVATE); break;
        }
        case Reload: loadSettings(false); break;
        }
        if (failed) { DestroyWindow(hwnd); return 0; }
        saveSettings(); requestFrame(); return 0;
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED: {
        RECT bounds{}; GetWindowRect(hwnd,&bounds); int x=bounds.left,y=bounds.top;
        clampPosition(x,y,renderer->width(),renderer->height());
        SetWindowPos(hwnd,topmost?HWND_TOPMOST:HWND_NOTOPMOST,x,y,0,0,SWP_NOSIZE|SWP_NOACTIVATE);
        cursorDirty=true; requestFrame(); return 0;
    }
    case WM_CLOSE: panel.close(true); DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        saveSettings(); for (UINT id=1;id<=4;++id) KillTimer(hwnd,id);
        if (frameSignal) CancelWaitableTimer(frameSignal);
        if (trayReady) Shell_NotifyIconW(NIM_DELETE,&tray);
        PostQuitMessage(0); return 0;
    default: return DefWindowProcW(hwnd,message,wparam,lparam);
    }
}

double processSeconds() {
    FILETIME created,exited,kernel,user;
    if (!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)) return 0;
    ULARGE_INTEGER k{},u{}; k.LowPart=kernel.dwLowDateTime; k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime; u.HighPart=user.dwHighDateTime;
    return (k.QuadPart+u.QuadPart)/10000000.;
}
void metrics(double cpuStart,std::uint64_t wallStart,DWORD gdiStart) {
    PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb=sizeof(memory);
    GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));
    double elapsed=std::max(0.001,(GetTickCount64()-wallStart)/1000.);
    std::ofstream out(root/(idleBenchmark?L"performance-idle.txt":benchmark?L"performance-active.txt":L"smoke-test.log"),std::ios::app);
    out<<std::fixed<<std::setprecision(3)
       <<"elapsed_seconds="<<elapsed<<"\nsingle_core_cpu_percent="<<(processSeconds()-cpuStart)/elapsed*100
       <<"\nworking_set_MB="<<memory.WorkingSetSize/1048576.
       <<"\nprivate_MB="<<memory.PrivateUsage/1048576.
       <<"\npeak_working_set_MB="<<memory.PeakWorkingSetSize/1048576.
       <<"\nframes="<<renderer->frames<<"\nbuffer_rebuilds="<<renderer->buffers
       <<"\nraw_events="<<rawEvents<<"\ngdi_start="<<gdiStart
       <<"\ngdi_end="<<GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<<'\n';
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int) {
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) SetProcessDPIAware();
    std::wstring arguments(command);
    smoke=arguments.find(L"--smoke-test")!=std::wstring::npos;
    selfTest=arguments.find(L"--self-test")!=std::wstring::npos;
    benchmark=arguments.find(L"--benchmark")!=std::wstring::npos;
    idleBenchmark=arguments.find(L"--idle-benchmark")!=std::wstring::npos;
    configTrace=arguments.find(L"--trace-config")!=std::wstring::npos;
    HANDLE singleton=nullptr;
    if (!profiling()) {
        singleton=CreateMutexW(nullptr,FALSE,L"Local\\BongoCatCppDemoV2");
        if (singleton && GetLastError()==ERROR_ALREADY_EXISTS) { CloseHandle(singleton); return 0; }
    }
    wchar_t executable[32768]{}; GetModuleFileNameW(nullptr,executable,32768);
    root=fs::path(executable).parent_path(); settings=root/L"settings.ini";
    QueryPerformanceFrequency(&clockFrequency);
    // Windows 10 1803+ supplies precise one-shot timers without changing global timer resolution.
    frameSignal=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_ALL_ACCESS);
    if (profiling() || configTrace) std::ofstream(root/L"smoke-test.log")<<"Starting v3 verification\n";
    if (benchmark || idleBenchmark) std::ofstream(root/(idleBenchmark?L"performance-idle.txt":L"performance-active.txt"));
    Gdiplus::GdiplusStartupInput startup; ULONG_PTR token=0;
    if (Gdiplus::GdiplusStartup(&token,&startup,nullptr)!=Gdiplus::Ok) return 1;
    WNDCLASSW type{}; type.lpfnWndProc=procedure; type.hInstance=instance;
    type.style=CS_DBLCLKS;
    type.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(100));
    type.lpszClassName=L"BongoCatCppDemoV2"; type.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    RegisterClassW(&type);
    if (!profiling()) loadSettings(true);
    RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    int height=static_cast<int>(std::lround(width*480./680.));
    int x=work.right-width-24,y=work.bottom-height-24;
    if (!profiling()) {
        x=static_cast<int>(GetPrivateProfileIntW(L"Window",L"X",x,settings.c_str()));
        y=static_cast<int>(GetPrivateProfileIntW(L"Window",L"Y",y,settings.c_str()));
    }
    clampPosition(x,y,width,height);
    window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,
                           type.lpszClassName,L"BongoCat · Keyboard & Mouse",WS_POPUP,
                           x,y,width,height,nullptr,nullptr,instance,nullptr);
    int result=0;
    renderer=std::make_unique<Renderer>();
    if (!type.hIcon || !window || !renderer->load(root/L"assets"/L"cat-atlas-v3.png",window) || !renderer->resize(width)) result=2;
    else if (selfTest) {
        result=renderer->draw(input,scene,GetTickCount64()) && renderer->save(root/L"preview.png")?0:3;
        std::ofstream(root/L"self-test.log")<<(result?"FAIL: icon/image/render\n":"PASS: embedded cat icon, four atlas layers, cached composition, transparent window\n");
    } else {
        RAWINPUTDEVICE devices[2]={{0x01,0x02,RIDEV_INPUTSINK|RIDEV_DEVNOTIFY,window},
                                   {0x01,0x06,RIDEV_INPUTSINK|RIDEV_DEVNOTIFY,window}};
        if (!RegisterRawInputDevices(devices,2,sizeof(RAWINPUTDEVICE))) result=4;
        else {
            note("PASS: background Raw Input registered");
            tray.cbSize=sizeof(tray); tray.hWnd=window; tray.uID=1; tray.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
            tray.uCallbackMessage=TrayMessage;
            // Shared resource icon is loaded once; Windows manages its lifetime.
            tray.hIcon=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(100),IMAGE_ICON,
                GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
            if (!tray.hIcon) tray.hIcon=type.hIcon;
            lstrcpyW(tray.szTip,L"BongoCat · 键盘与鼠标 · 点击打开菜单");
            taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
            trayReady=Shell_NotifyIconW(NIM_ADD,&tray)!=FALSE;
            if (!trayReady) note("NOTE: tray unavailable; click-through disabled");
            renderer->setOpacity(opacity);
            applyStyle(); sampleCursor(); scene.mouseX=targetX; scene.mouseY=targetY;
            if (!profiling()) {
                std::error_code error;
                if (!fs::exists(settings,error) && !error) saveSettings();
            }
            if (!renderer->draw(input,scene,GetTickCount64())) result=5;
            else {
                ShowWindow(window,SW_SHOWNOACTIVATE);
                if (arguments.find(L"--settings")!=std::wstring::npos) openSettings();
                double cpuStart=processSeconds(); auto wallStart=GetTickCount64();
                DWORD gdiStart=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
                if (!SetTimer(window,2,1000,nullptr)) result=6;
                if (smoke && !SetTimer(window,3,40,nullptr)) result=6;
                if ((benchmark || idleBenchmark) && !SetTimer(window,4,10000,nullptr)) result=6;
                if (!result) {
                    if (benchmark) requestFrame();
                    bool running=true;
                    while (running) {
                        DWORD ready=MsgWaitForMultipleObjectsEx(frameSignal?1:0,&frameSignal,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
                        if (ready==WAIT_FAILED) { failed=true; break; }
                        if (frameSignal && ready==WAIT_OBJECT_0) { frameTimer=false; drawFrame(); }
                        MSG event{};
                        // Bound each message batch so high-rate mice cannot starve drawing.
                        for (int batch=0;batch<64 && PeekMessageW(&event,nullptr,0,0,PM_REMOVE);++batch) {
                            if (event.message==WM_QUIT) { running=false; break; }
                            if (!panel.navigate(event)) { TranslateMessage(&event); DispatchMessageW(&event); }
                        }
                    }
                    if (failed) result=7;
                    if (profiling()) metrics(cpuStart,wallStart,gdiStart);
                }
            }
        }
    }
    if (result) {
        note("FAIL: initialization or message loop");
        if (!profiling()) MessageBoxW(window,L"无法启动。请保留 assets/cat-atlas-v3.png，并查看文件权限。",L"BongoCat",MB_ICONERROR);
    }
    if (window && IsWindow(window)) DestroyWindow(window);
    panel.close(false);
    renderer.reset(); Gdiplus::GdiplusShutdown(token);
    if (frameSignal) CloseHandle(frameSignal);
    if (singleton) CloseHandle(singleton);
    return result;
}
