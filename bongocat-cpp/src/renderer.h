#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <gdiplus.h>
#include <filesystem>
#include <memory>
#include <vector>
#include "input_state.h"

struct Scene {
    float mouseX = 0, mouseY = 0;
    int wheel = 0;
    std::uint64_t wheelUntil = 0;
};

class Renderer {
    HWND window_ = nullptr;
    std::unique_ptr<Gdiplus::Bitmap> atlas_;
    Gdiplus::Rect parts_[4];
    HDC dc_ = nullptr;
    HBITMAP dib_ = nullptr;
    HGDIOBJ old_ = nullptr;
    void* pixels_ = nullptr;
    int width_ = 0, height_ = 0;
    BYTE opacity_=255;
    float scale_ = 1;
    std::vector<BYTE> background_;
    std::unique_ptr<Gdiplus::Bitmap> surface_, paw_, mouse_;
    std::vector<std::unique_ptr<Gdiplus::Bitmap>> pressedKeys_;
    void releaseBuffer();
    void drawPart(Gdiplus::Graphics&, int, const Gdiplus::RectF&);
    void keycap(Gdiplus::Graphics&, const KeyCap&, bool);
public:
    std::uint64_t frames = 0, buffers = 0;
    ~Renderer();
    bool load(const std::filesystem::path&, HWND);
    bool resize(int width);
    bool draw(const InputState&, const Scene&, std::uint64_t now);
    bool save(const std::filesystem::path&);
    int width() const { return width_; }
    int height() const { return height_; }
    void setOpacity(int percent) { opacity_=static_cast<BYTE>(percent*255/100); }
};
