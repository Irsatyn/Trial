$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path $projectRoot 'build/config-smoke'
New-Item -ItemType Directory -Force "$testRoot/assets" | Out-Null
Copy-Item -LiteralPath "$projectRoot/dist/BongoCatDemo.exe" -Destination "$testRoot/BongoCatDemo.exe" -Force
Copy-Item -LiteralPath "$projectRoot/assets/cat-atlas-v3.png" -Destination "$testRoot/assets/cat-atlas-v3.png" -Force
@'
[Window]
Width=999999
ClickThrough=0
Paused=0
[Animation]
MaxFPS=0
MinPressMs=-5
SmoothingMs=999999
'@ | Set-Content -LiteralPath "$testRoot/settings.ini" -Encoding ascii
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CatWindowTest {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
    public static IntPtr FindCat() { return FindWindow("BongoCatCppDemoV2", null); }
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, UIntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
}
'@
[void][CatWindowTest]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
$testProcess = Start-Process -FilePath "$testRoot/BongoCatDemo.exe" -ArgumentList '--trace-config' -WindowStyle Hidden -PassThru
try {
    $testHandle = [IntPtr]::Zero
    for ($attempt=0; $attempt -lt 100; $attempt++) {
        $candidate = [CatWindowTest]::FindCat()
        [uint32]$candidateId = 0
        if ($candidate -ne [IntPtr]::Zero) {
            [void][CatWindowTest]::GetWindowThreadProcessId($candidate, [ref]$candidateId)
            if ($candidateId -eq $testProcess.Id) { $testHandle=$candidate; break }
        }
        Start-Sleep -Milliseconds 100
    }
    if ($testHandle -eq [IntPtr]::Zero) {
        $testProcess.Refresh()
        throw "Test window missing: process=$($testProcess.Id), exited=$($testProcess.HasExited), exit=$($testProcess.ExitCode), candidate=$candidate, owner=$candidateId, title=$($testProcess.MainWindowTitle)"
    }
    $rect = [CatWindowTest+Rect]::new()
    [void][CatWindowTest]::GetWindowRect($testHandle, [ref]$rect)
    if (($rect.Right-$rect.Left) -ne 1020) { throw "Initial width clamp failed: $($rect.Right-$rect.Left)" }
    @'
[Window]
Width=408
X=30
Y=40
ClickThrough=0
Paused=0
[Animation]
MaxFPS=60
MinPressMs=50
SmoothingMs=60
'@ | Set-Content -LiteralPath "$testRoot/settings.ini" -Encoding ascii
    for ($attempt=0; $attempt -lt 50; $attempt++) {
        Start-Sleep -Milliseconds 100
        [void][CatWindowTest]::GetWindowRect($testHandle, [ref]$rect)
        if (($rect.Right-$rect.Left) -eq 408) { break }
    }
    if (($rect.Right-$rect.Left) -ne 408) {
        Get-Content -LiteralPath "$testRoot/smoke-test.log"
        throw "INI hot reload did not resize window: actual=$($rect.Right-$rect.Left)"
    }
    if ($rect.Left -ne 30 -or $rect.Top -ne 40) { throw 'INI hot reload did not apply coordinates' }
    [void][CatWindowTest]::PostMessage($testHandle, 0x111, [UIntPtr]::new([uint64]101), [IntPtr]::Zero)
    Start-Sleep -Milliseconds 100
    [void][CatWindowTest]::PostMessage($testHandle, 0x10, [UIntPtr]::Zero, [IntPtr]::Zero)
    if (!$testProcess.WaitForExit(3000)) { throw 'Clean shutdown failed' }
    $saved = Get-Content -LiteralPath "$testRoot/settings.ini" -Raw
    if ($testProcess.ExitCode -ne 0 -or $saved -notmatch 'Paused=1' -or $saved -notmatch 'Width=408') { throw 'Settings persistence failed' }
    Write-Output 'PASS: config clamp, hot reload, pause persistence, orderly shutdown'
} finally {
    $testProcess.Refresh()
    if (!$testProcess.HasExited) { Stop-Process -Id $testProcess.Id }
}
