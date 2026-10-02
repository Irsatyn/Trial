$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path $projectRoot 'build/gui-smoke'
New-Item -ItemType Directory -Force "$testRoot/assets" | Out-Null
Copy-Item -LiteralPath "$projectRoot/dist/BongoCatDemo.exe" -Destination "$testRoot/BongoCatDemo.exe" -Force
Copy-Item -LiteralPath "$projectRoot/assets/cat-atlas-v3.png" -Destination "$testRoot/assets/cat-atlas-v3.png" -Force
@'
[Window]
Width=612
X=30
Y=40
Opacity=100
Topmost=1
ClickThrough=0
Paused=0
[Animation]
MaxFPS=60
MinPressMs=50
SmoothingMs=60
MouseRange=22
FollowMouse=1
'@ | Set-Content -LiteralPath "$testRoot/settings.ini" -Encoding ascii
Add-Type -ReferencedAssemblies System.Drawing,System.Windows.Forms @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;
using System.Runtime.InteropServices;
public static class CatGuiTest {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr FindWindow(string cls, string title);
    public static IntPtr Cat() { return FindWindow("BongoCatCppDemoV2",null); }
    public static IntPtr Panel() { return FindWindow("BongoCatSettingsPanel",null); }
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint id);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd,out Rect rect);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd,IntPtr after,int x,int y,int w,int h,uint flags);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd,int id);
    [DllImport("user32.dll",EntryPoint="SendMessageW")] public static extern IntPtr SendMessage(IntPtr hwnd,uint msg,IntPtr w,IntPtr l);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd,uint msg,IntPtr w,IntPtr l);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd,IntPtr dc,uint flags);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd,int cmd);
    [DllImport("user32.dll")] public static extern bool UpdateWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern uint GetGuiResources(IntPtr process,uint flags);
    public static void Slider(IntPtr panel,int id,int value) {
        IntPtr control=GetDlgItem(panel,id);
        SendMessage(control,0x405,new IntPtr(1),new IntPtr(value));
        SendMessage(panel,0x114,new IntPtr(8),control);
    }
    public static void Click(IntPtr panel,int id) { SendMessage(GetDlgItem(panel,id),0xF5,IntPtr.Zero,IntPtr.Zero); }
    public static void Open(IntPtr cat) { SendMessage(cat,0x111,new IntPtr(108),IntPtr.Zero); }
    public static int Width(IntPtr hwnd) { Rect r; GetWindowRect(hwnd,out r); return r.Right-r.Left; }
    public static Rect Bounds(IntPtr hwnd) { Rect r; GetWindowRect(hwnd,out r); return r; }
    public static void BottomRight(IntPtr hwnd) {
        Rect r=Bounds(hwnd); Rectangle area=Screen.FromHandle(hwnd).WorkingArea;
        SetWindowPos(hwnd,IntPtr.Zero,area.Right-(r.Right-r.Left),area.Bottom-(r.Bottom-r.Top),0,0,0x15);
    }
    public static void Capture(IntPtr hwnd,string path) {
        ShowWindow(hwnd,1); UpdateWindow(hwnd);
        Rect r; GetWindowRect(hwnd,out r);
        using(Bitmap image=new Bitmap(r.Right-r.Left,r.Bottom-r.Top,PixelFormat.Format24bppRgb)) {
            using(Graphics g=Graphics.FromImage(image)) {
                IntPtr dc=g.GetHdc();
                try { if(!PrintWindow(hwnd,dc,2)) throw new Exception("GUI PrintWindow failed"); }
                finally { g.ReleaseHdc(dc); }
            }
            image.Save(path,ImageFormat.Png);
        }
    }
}
'@
[void][CatGuiTest]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
$guiProcess = Start-Process -FilePath "$testRoot/BongoCatDemo.exe" -ArgumentList '--settings' -WindowStyle Hidden -PassThru
try {
    $cat=[IntPtr]::Zero; $gui=[IntPtr]::Zero
    for ($attempt=0;$attempt -lt 60;$attempt++) {
        $cat=[CatGuiTest]::Cat(); $gui=[CatGuiTest]::Panel()
        [uint32]$ownerId=0
        if ($gui -ne [IntPtr]::Zero) {
            [void][CatGuiTest]::GetWindowThreadProcessId($gui,[ref]$ownerId)
            if ($ownerId -eq $guiProcess.Id -and [CatGuiTest]::GetDlgItem($gui,302) -ne [IntPtr]::Zero) { break }
        }
        Start-Sleep -Milliseconds 100
    }
    if ($ownerId -ne $guiProcess.Id) { throw 'Settings panel did not open for test process' }
    Start-Sleep -Milliseconds 150
    [CatGuiTest]::Capture($gui,(Join-Path $projectRoot 'dist/gui-preview.png'))
    [CatGuiTest]::Slider($gui,200,816)
    [CatGuiTest]::Slider($gui,201,55)
    [CatGuiTest]::Slider($gui,205,35)
    if ([CatGuiTest]::Width($cat) -ne 816) { throw 'Size preview failed' }
    $previewIni=Get-Content -LiteralPath "$testRoot/settings.ini" -Raw
    if ($previewIni -notmatch 'Width=612' -or $previewIni -notmatch 'Opacity=100') { throw 'Unsaved preview changed disk settings' }
    [CatGuiTest]::Click($gui,301)
    if ([CatGuiTest]::Width($cat) -ne 612 -or [CatGuiTest]::Panel() -ne [IntPtr]::Zero) { throw 'Cancel did not restore baseline' }
    [CatGuiTest]::BottomRight($cat)
    $originalBounds=[CatGuiTest]::Bounds($cat)
    [CatGuiTest]::Open($cat); $gui=[CatGuiTest]::Panel()
    [CatGuiTest]::Slider($gui,200,1020)
    [CatGuiTest]::Click($gui,301)
    $restoredBounds=[CatGuiTest]::Bounds($cat)
    if ($originalBounds.Left -ne $restoredBounds.Left -or $originalBounds.Top -ne $restoredBounds.Top) { throw 'Cancel failed to restore edge position' }
    [CatGuiTest]::Open($cat); $gui=[CatGuiTest]::Panel()
    [CatGuiTest]::Slider($gui,201,50); [CatGuiTest]::Click($gui,300)
    if ([CatGuiTest]::Width($cat) -ne 612) { throw 'Restore defaults preview failed' }
    [void][CatGuiTest]::SendMessage($gui,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    $gdiBaseline=[CatGuiTest]::GetGuiResources($guiProcess.Handle,0)
    for ($cycle=0;$cycle -lt 4;$cycle++) {
        [CatGuiTest]::Open($cat); $gui=[CatGuiTest]::Panel(); [CatGuiTest]::Click($gui,301)
    }
    $gdiAfter=[CatGuiTest]::GetGuiResources($guiProcess.Handle,0)
    if ($gdiAfter -gt $gdiBaseline) { throw "Panel resource leak: $gdiBaseline -> $gdiAfter" }
    [CatGuiTest]::Open($cat); $gui=[CatGuiTest]::Panel()
    [CatGuiTest]::Slider($gui,200,408); [CatGuiTest]::Slider($gui,201,80)
    [CatGuiTest]::Slider($gui,202,30); [CatGuiTest]::Slider($gui,205,30)
    [CatGuiTest]::Click($gui,210); [CatGuiTest]::Click($gui,211); [CatGuiTest]::Click($gui,213)
    [CatGuiTest]::Click($gui,302)
    $saved=Get-Content -LiteralPath "$testRoot/settings.ini" -Raw
    foreach ($setting in @('Width=408','Opacity=80','MaxFPS=30','MouseRange=30','FollowMouse=0','Topmost=0','Paused=1')) {
        if ($saved -notmatch [regex]::Escape($setting)) { throw "Save failed for $setting" }
    }
    [void][CatGuiTest]::PostMessage($cat,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    if (!$guiProcess.WaitForExit(3000) -or $guiProcess.ExitCode -ne 0) { throw 'GUI test app did not close cleanly' }
    Write-Output "PASS: GUI preview, cancel rollback, save, panel reopen, stable GDI objects ($gdiBaseline -> $gdiAfter), clean exit"
} finally {
    $guiProcess.Refresh()
    if (!$guiProcess.HasExited) { Stop-Process -Id $guiProcess.Id }
}
