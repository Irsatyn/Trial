$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$projectRoot=Split-Path $PSScriptRoot -Parent
$iconSizes=@(16,20,24,32,40,48,64,128,256)
$images=@()
foreach($size in $iconSizes) {
    $canvas=New-Object System.Drawing.Bitmap ($size*4),($size*4)
    $graphics=[System.Drawing.Graphics]::FromImage($canvas)
    $graphics.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.ScaleTransform(($size*4/64.0),($size*4/64.0))
    $outline=New-Object System.Drawing.Drawing2D.GraphicsPath
    $outline.AddBezier(6,30,5,20,7,6,10,5)
    $outline.AddLine(10,5,24,15)
    $outline.AddBezier(24,15,29,13,35,13,40,15)
    $outline.AddLine(40,15,54,5)
    $outline.AddBezier(54,5,57,6,59,20,58,30)
    $outline.AddBezier(58,30,64,53,48,59,32,59)
    $outline.AddBezier(32,59,16,59,0,53,6,30)
    $outline.CloseFigure()
    $pen=New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255,25,25,28)),3.5
    $pen.LineJoin=[System.Drawing.Drawing2D.LineJoin]::Round
    $graphics.FillPath([System.Drawing.Brushes]::White,$outline)
    $graphics.DrawPath($pen,$outline)
    $graphics.FillEllipse([System.Drawing.Brushes]::Black,19,30,6,9)
    $graphics.FillEllipse([System.Drawing.Brushes]::Black,39,30,6,9)
    $graphics.DrawBezier($pen,25,43,25,49,31,50,32,45)
    $graphics.DrawBezier($pen,32,45,33,50,39,49,39,43)
    $graphics.Dispose(); $outline.Dispose(); $pen.Dispose()
    $output=New-Object System.Drawing.Bitmap $size,$size
    $resize=[System.Drawing.Graphics]::FromImage($output)
    $resize.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $resize.DrawImage($canvas,0,0,$size,$size)
    $resize.Dispose(); $canvas.Dispose()
    $stream=New-Object System.IO.MemoryStream
    $output.Save($stream,[System.Drawing.Imaging.ImageFormat]::Png)
    $images+=,@{Size=$size;Bytes=$stream.ToArray()}
    if($size -eq 256){$output.Save((Join-Path $projectRoot 'assets/tray-cat-preview.png'),[System.Drawing.Imaging.ImageFormat]::Png)}
    $stream.Dispose(); $output.Dispose()
}
$file=[System.IO.File]::Create((Join-Path $projectRoot 'assets/tray-cat.ico'))
$writer=New-Object System.IO.BinaryWriter $file
try {
    $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$images.Count)
    $offset=6+16*$images.Count
    foreach($frame in $images){
        $dimension=if($frame.Size -eq 256){0}else{$frame.Size}
        $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
        $writer.Write([byte]0); $writer.Write([byte]0)
        $writer.Write([uint16]1); $writer.Write([uint16]32)
        $writer.Write([uint32]$frame.Bytes.Length); $writer.Write([uint32]$offset)
        $offset+=$frame.Bytes.Length
    }
    foreach($frame in $images){$writer.Write([byte[]]$frame.Bytes)}
} finally {$writer.Dispose(); $file.Dispose()}
Write-Output 'Created assets/tray-cat.ico (16-256px, transparent cat face)'
