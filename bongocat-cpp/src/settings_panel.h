#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>
#include <vector>
#include "panel_settings.h"

class SettingsPanel {
    struct Placement { HWND child; int x,y,w,h,style; };
    HWND hwnd_=nullptr;
    HWND owner_=nullptr;
    RECT baselineBounds_{};
    float scale_=1;
    HFONT normal_=nullptr,title_=nullptr,section_=nullptr;
    HBRUSH background_=nullptr;
    bool trayAvailable_=false;
    PanelSettings baseline_,applied_;
    std::function<void(const PanelSettings&,bool)> apply_;
    std::vector<Placement> controls_;
    static LRESULT CALLBACK procedure(HWND,UINT,WPARAM,LPARAM);
    LRESULT message(UINT,WPARAM,LPARAM);
    HWND child(const wchar_t*,const wchar_t*,DWORD,int,int,int,int,int,int font=0);
    void build();
    void populate(const PanelSettings&);
    void updateValues();
    void preview(bool deferWidth=false);
    PanelSettings read() const;
    void fonts();
    void relayout();
    int px(int value) const;
public:
    ~SettingsPanel();
    void show(HWND,const PanelSettings&,bool,std::function<void(const PanelSettings&,bool)>);
    void close(bool restore=true);
    bool isOpen() const { return hwnd_ && IsWindow(hwnd_); }
    HWND handle() const { return hwnd_; }
    bool navigate(MSG& event) { return isOpen() && IsDialogMessageW(hwnd_,&event); }
};
