param([string]$Compiler = 'C:\msys64\ucrt64\bin\g++.exe')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    if (!(Test-Path -LiteralPath $Compiler)) { throw "Compiler not found: $Compiler. Supply -Compiler with a MinGW g++ path." }
    New-Item -ItemType Directory -Force 'build', 'dist/assets' | Out-Null
    & $Compiler -std=c++17 -O2 -Wall -Wextra -static 'tests/animation_test.cpp' -o 'build/animation_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
    & './build/animation_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Animation tests failed' }
    & $Compiler -std=c++17 -O2 -Wall -Wextra -static 'tests/input_test.cpp' -o 'build/input_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Input test compilation failed' }
    & './build/input_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Input tests failed' }
    $resourceCompiler = Join-Path (Split-Path $Compiler -Parent) 'windres.exe'
    & $resourceCompiler 'app.rc' -o 'build/app-res.o'
    if ($LASTEXITCODE -ne 0) { throw 'Manifest compilation failed' }
    & $Compiler -std=c++17 -O2 -Wall -Wextra -municode -mwindows -static 'src/main.cpp' 'src/renderer.cpp' 'src/settings_panel.cpp' 'build/app-res.o' -o 'dist/BongoCatDemo.exe' -lgdiplus -lgdi32 -luser32 -lshell32 -lpsapi -lcomctl32
    if ($LASTEXITCODE -ne 0) { throw 'Application compilation failed' }
    Copy-Item -LiteralPath 'assets/cat-atlas-v3.png' -Destination 'dist/assets/cat-atlas-v3.png' -Force
    Copy-Item -LiteralPath 'README.md' -Destination 'dist/README.md' -Force
    Copy-Item -LiteralPath 'settings.example.ini' -Destination 'dist/settings.example.ini' -Force
    if (Test-Path -LiteralPath 'PERFORMANCE.md') { Copy-Item -LiteralPath 'PERFORMANCE.md' -Destination 'dist/PERFORMANCE.md' -Force }
    Write-Host 'Built dist/BongoCatDemo.exe'
} finally { Pop-Location }
