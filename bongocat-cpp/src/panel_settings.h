#pragma once
#include <algorithm>

struct PanelSettings {
    int width=612, opacity=100, fps=60, holdMs=50, smoothingMs=60, mouseRange=22;
    bool followMouse=true, topmost=true, clickThrough=false, paused=false;
    void sanitize() {
        width=std::clamp(width,384,1020); opacity=std::clamp(opacity,30,100);
        fps=std::clamp(fps,30,120); holdMs=std::clamp(holdMs,10,200);
        smoothingMs=std::clamp(smoothingMs,10,200); mouseRange=std::clamp(mouseRange,0,40);
    }
};
