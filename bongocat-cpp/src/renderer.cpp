#include "renderer.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cwchar>

using namespace Gdiplus;
namespace {
void keyboardTransform(Graphics& g) {
    g.TranslateTransform(414,362); g.RotateTransform(187); g.TranslateTransform(-414,-362);
}
PointF keyboardPoint(float x,float y) {
    constexpr float angle=187.f*3.14159265358979323846f/180;
    float dx=x-414,dy=y-362;
    return {414+dx*std::cos(angle)-dy*std::sin(angle),362+dx*std::sin(angle)+dy*std::cos(angle)};
}
void rounded(Graphics& g, float x, float y, float w, float h, float radius,
             Color fill, Color edge, float stroke = 1) {
    GraphicsPath p;
    float d = radius * 2;
    p.AddArc(x,y,d,d,180,90); p.AddArc(x+w-d,y,d,d,270,90);
    p.AddArc(x+w-d,y+h-d,d,d,0,90); p.AddArc(x,y+h-d,d,d,90,90); p.CloseFigure();
    SolidBrush b(fill); Pen pen(edge,stroke);
    g.FillPath(&b,&p); g.DrawPath(&pen,&p);
}
}

void Renderer::releaseBuffer() {
    surface_.reset(); paw_.reset(); mouse_.reset();
    pressedKeys_.clear();
    if (dc_ && old_) SelectObject(dc_, old_);
    if (dib_) DeleteObject(dib_);
    if (dc_) DeleteDC(dc_);
    dc_ = nullptr; dib_ = nullptr; old_ = nullptr; pixels_ = nullptr;
}
Renderer::~Renderer() { releaseBuffer(); }

bool Renderer::load(const std::filesystem::path& file, HWND window) {
    window_ = window;
    atlas_ = std::make_unique<Bitmap>(file.c_str());
    if (atlas_->GetLastStatus() != Ok || atlas_->GetWidth() < 16 ||
        atlas_->GetHeight() != atlas_->GetWidth() || atlas_->GetWidth() % 2) return false;
    Color corner;
    if (atlas_->GetPixel(0,0,&corner)!=Ok || corner.GetA()!=0) return false;
    Rect all(0,0,atlas_->GetWidth(),atlas_->GetHeight());
    BitmapData lock{};
    if (atlas_->LockBits(&all,ImageLockModeRead,PixelFormat32bppARGB,&lock) != Ok) return false;
    int half = static_cast<int>(atlas_->GetWidth()/2);
    bool valid = true;
    for (int i=0; i<4; ++i) {
        int x0=(i%2)*half, y0=(i/2)*half;
        int left=x0+half, top=y0+half, right=x0, bottom=y0;
        for (int y=y0; y<y0+half; ++y) {
            const auto* row = static_cast<const BYTE*>(lock.Scan0) + y*lock.Stride;
            for (int x=x0; x<x0+half; ++x) if (row[x*4+3] > 32) {
                left=std::min(left,x); top=std::min(top,y);
                right=std::max(right,x); bottom=std::max(bottom,y);
            }
        }
        if (right<=left || bottom<=top) { valid=false; break; }
        parts_[i]=Rect(left,top,right-left+1,bottom-top+1);
    }
    atlas_->UnlockBits(&lock);
    return valid;
}

void Renderer::drawPart(Graphics& g, int part, const RectF& destination) {
    const auto& r = parts_[part];
    g.DrawImage(atlas_.get(),destination,static_cast<REAL>(r.X),static_cast<REAL>(r.Y),
                static_cast<REAL>(r.Width),static_cast<REAL>(r.Height),UnitPixel);
}

void Renderer::keycap(Graphics& g, const KeyCap& k, bool pressed) {
    // Local upward offsets become visible front edges after the keyboard rotation.
    rounded(g,k.x,k.y-2,k.w,k.h+2,3,
            pressed?Color(255,199,103,130):Color(255,161,167,176),Color(255,91,96,106));
    float y=k.y+(pressed?-1.f:0.f);
    rounded(g,k.x,y,k.w,k.h-2,3,
            pressed ? Color(255,255,166,190) : Color(255,249,249,251),
            pressed ? Color(255,185,74,107) : Color(255,91,96,106));
    Pen highlight(pressed?Color(255,255,206,221):Color(255,255,255,255),1);
    g.DrawLine(&highlight,k.x+4,y+k.h-4,k.x+k.w-4,y+k.h-4);
    const float fontSize = k.w<35 && std::wcslen(k.label)>2 ? 8.f : (k.w<35?11.f:9.5f);
    Font font(L"Segoe UI",fontSize,FontStyleBold,UnitPixel);
    SolidBrush ink(Color(255,48,51,60));
    StringFormat format; format.SetAlignment(StringAlignmentCenter); format.SetLineAlignment(StringAlignmentCenter);
    format.SetFormatFlags(StringFormatFlagsNoWrap);
    RectF rect(k.x,y,k.w,k.h-2);
    g.DrawString(k.label,-1,&font,rect,&format,&ink);
}

bool Renderer::resize(int width) {
    if (!atlas_) return false;
    releaseBuffer();
    width_ = width; height_ = static_cast<int>(std::lround(width*480./680.)); scale_=width/680.f;
    HDC screen=GetDC(nullptr); dc_=CreateCompatibleDC(screen);
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width_; info.bmiHeader.biHeight=-height_;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    dib_=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels_,nullptr,0);
    ReleaseDC(nullptr,screen);
    if (!dc_ || !dib_ || !pixels_) { releaseBuffer(); return false; }
    old_=SelectObject(dc_,dib_);
    surface_=std::make_unique<Bitmap>(width_,height_,width_*4,PixelFormat32bppPARGB,static_cast<BYTE*>(pixels_));
    {
        Graphics g(surface_.get()); g.Clear(Color(0,0,0,0));
        g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.ScaleTransform(scale_,scale_);
        drawPart(g,0,RectF(184,28,400,340));
        PointF desk[]={{16,248},{664,304},{664,476},{16,476}};
        SolidBrush white(Color(255,255,255,255)); g.FillPolygon(&white,desk,4);
        Pen edge(Color(255,20,20,20),5); g.DrawLine(&edge,desk[0],desk[1]);
        PointF mat[]={{39,283},{201,310},{184,416},{22,390}};
        SolidBrush gray(Color(255,181,182,184)); g.FillPolygon(&gray,mat,4);
        auto saved=g.Save(); keyboardTransform(g);
        // Tight bezel, shallow front wall and contact shadow are cached with the background.
        rounded(g,197,283,430,132,15,Color(40,25,25,28),Color(0,0,0,0),0);
        rounded(g,195,286,430,132,14,Color(255,155,161,171),Color(255,42,45,51),3);
        rounded(g,195,294,430,128,14,Color(255,231,234,240),Color(255,42,45,51),3);
        rounded(g,199,298,422,120,11,Color(255,243,244,247),Color(255,255,255,255),1);
        for (const auto& k : keyLayout()) keycap(g,k,false);
        g.Restore(saved);
    }
    background_.resize(static_cast<std::size_t>(width_)*height_*4);
    std::memcpy(background_.data(),pixels_,background_.size());
    // Small prepared sprites are rasterized once at each output size.
    auto prepare=[this](int part,int w,int h) {
        auto image=std::make_unique<Bitmap>(w,h,PixelFormat32bppPARGB);
        Graphics g(image.get()); g.Clear(Color(0,0,0,0));
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        drawPart(g,part,RectF(0,0,static_cast<float>(w),static_cast<float>(h)));
        return image;
    };
    paw_=prepare(3,static_cast<int>(48*scale_),static_cast<int>(43*scale_));
    mouse_=prepare(2,static_cast<int>(64*scale_),static_cast<int>(90*scale_));
    // Pressed keycaps include their labels; cache them instead of rasterizing text every frame.
    for (const auto& key : keyLayout()) {
        int w=static_cast<int>(std::ceil((key.w+4)*scale_));
        int h=static_cast<int>(std::ceil((key.h+5)*scale_));
        auto image=std::make_unique<Bitmap>(w,h,PixelFormat32bppPARGB);
        Graphics g(image.get()); g.Clear(Color(0,0,0,0));
        g.SetSmoothingMode(SmoothingModeAntiAlias); g.ScaleTransform(scale_,scale_);
        auto local=key; local.x=2; local.y=2;
        keycap(g,local,true);
        pressedKeys_.push_back(std::move(image));
    }
    ++buffers;
    return true;
}

bool Renderer::draw(const InputState& state,const Scene& scene,std::uint64_t now) {
    if (!surface_) return false;
    std::memcpy(pixels_,background_.data(),background_.size());
    {
        Graphics g(surface_.get()); g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetInterpolationMode(InterpolationModeNearestNeighbor);
        g.ScaleTransform(scale_,scale_);
        const auto& keys=keyLayout();
        auto saved=g.Save(); keyboardTransform(g);
        for (std::size_t i=0;i<keys.size();++i) if (state.visible(keys[i].vk,now)) {
            const auto& k=keys[i]; auto* cached=pressedKeys_[i].get();
            g.DrawImage(cached,RectF(k.x-2,k.y-2,cached->GetWidth()/scale_,cached->GetHeight()/scale_));
        }
        g.Restore(saved);
        const float mx=scene.mouseX,my=scene.mouseY;
        saved=g.Save();
        g.TranslateTransform(136+mx,333+my); g.RotateTransform(180);
        g.TranslateTransform(-136-mx,-333-my);
        g.DrawImage(mouse_.get(),RectF(104+mx,288+my,64,90));
        for (unsigned b : {1u,2u}) if (state.visible(b,now))
            rounded(g,116+mx+(b==2?24:0),302+my,15,21,5,Color(170,255,139,174),Color(255,192,76,112),1.2f);
        if (state.visible(4,now) || now<scene.wheelUntil)
            rounded(g,132+mx,303+my,9,17,3,Color(255,255,177,110),Color(255,145,79,42));
        g.Restore(saved);
        PointF targets[2]={{136+mx,321+my},{535,240}};
        int active=state.latestAny(now);
        if (active>=0) { const auto* key=findKey(active); targets[1]=keyboardPoint(key->x+key->w/2,key->y+key->h/2); }
        for (int hand=0;hand<2;++hand) {
            PointF start(hand==0?245.f:484.f,hand==0?226.f:240.f);
            PointF end=targets[hand];
            Pen outline(Color(255,25,25,28),23),fur(Color(255,255,255,255),17);
            outline.SetStartCap(LineCapRound); outline.SetEndCap(LineCapRound);
            fur.SetStartCap(LineCapRound); fur.SetEndCap(LineCapRound);
            float bulge=hand==0?-26.f:30.f;
            g.DrawBezier(&outline,start,PointF(start.X+bulge,start.Y+45),PointF(end.X+bulge,end.Y-35),end);
            g.DrawBezier(&fur,start,PointF(start.X+bulge,start.Y+45),PointF(end.X+bulge,end.Y-35),end);
            g.DrawImage(paw_.get(),RectF(end.X-24,end.Y-21,48,43));
            if (hand==1 && active<0) {
                SolidBrush pads(Color(255,244,141,166));
                g.FillEllipse(&pads,end.X-7,end.Y+2,14.f,13.f);
                for (float x : {-11.f,0.f,11.f}) g.FillEllipse(&pads,end.X+x-3,end.Y-10,6.f,8.f);
            }
        }
    }
    POINT origin{}; SIZE dimensions{width_,height_}; BLENDFUNCTION blend{AC_SRC_OVER,0,opacity_,AC_SRC_ALPHA};
    HDC screen=GetDC(nullptr);
    bool ok=UpdateLayeredWindow(window_,screen,nullptr,&dimensions,dc_,&origin,0,&blend,ULW_ALPHA)!=FALSE;
    ReleaseDC(nullptr,screen); ++frames;
    return ok;
}

bool Renderer::save(const std::filesystem::path& path) {
    const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
    return surface_ && surface_->Save(path.c_str(),&png,nullptr)==Ok;
}
