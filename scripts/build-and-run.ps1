param(
    [string]$BuildDir = "build",
    [string]$BuildType = "Debug",
    [string]$QtCMakePath = "C:/Qt/6.9.3/mingw_64/lib/cmake",
    [string]$QtMinGwBin = "C:/Qt/Tools/mingw1310_64/bin",
    [string]$CMakeExe = "C:/msys64/mingw64/bin/cmake.exe",
    [string]$NinjaExe = "C:/msys64/mingw64/bin/ninja.exe"
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptRoot
Push-Location $repoRoot

try {
    $running = Get-Process -Name "WinTools" -ErrorAction SilentlyContinue
    if ($running) {
        Write-Host "Stopping running WinTools.exe..." -ForegroundColor Yellow
        $running | Stop-Process -Force
        Start-Sleep -Milliseconds 300
    }

    if (-not (Test-Path $CMakeExe)) {
        $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
        if (-not $cmakeCommand) {
            throw "cmake is not available. Set -CMakeExe to a valid cmake.exe path."
        }
        $CMakeExe = $cmakeCommand.Source
    }
    if (-not (Test-Path $NinjaExe)) {
        throw "ninja not found at: $NinjaExe"
    }

    $gccPath = Join-Path $QtMinGwBin "gcc.exe"
    $gxxPath = Join-Path $QtMinGwBin "g++.exe"
    if (-not (Test-Path $gccPath)) {
        throw "gcc not found at: $gccPath"
    }
    if (-not (Test-Path $gxxPath)) {
        throw "g++ not found at: $gxxPath"
    }

    $pathParts = @($QtMinGwBin)
    if (Test-Path "C:/msys64/mingw64/bin") {
        $pathParts += "C:/msys64/mingw64/bin"
    }
    if (Test-Path "C:/msys64/usr/bin") {
        $pathParts += "C:/msys64/usr/bin"
    }
    $pathParts += ($env:PATH -split ';' | Where-Object {
        $_ -and
        ($_ -notmatch '^C:\\Qt\\Tools\\mingw1310_64\\bin$') -and
        ($_ -notmatch '^C:\\msys64\\mingw64\\bin$') -and
        ($_ -notmatch '^C:\\Qt\\6\.9\.3\\mingw_64\\bin$')
    })
    $env:PATH = ($pathParts | Select-Object -Unique) -join ';'

    $configureArgs = @(
        "--fresh",
        "-S", ".",
        "-B", $BuildDir,
        "-G", "Ninja",
        "-DCMAKE_PREFIX_PATH=$QtCMakePath",
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DCMAKE_MAKE_PROGRAM=$NinjaExe",
        "-DCMAKE_C_COMPILER=$gccPath",
        "-DCMAKE_CXX_COMPILER=$gxxPath"
    )

    Write-Host "Configuring CMake..." -ForegroundColor Cyan
    & $CMakeExe @configureArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed."
    }

    Write-Host "Building WinTools target..." -ForegroundColor Cyan
    $buildLogPath = Join-Path $BuildDir "last-build.log"
    & $CMakeExe --build $BuildDir --target WinTools -- -v *>&1 | Tee-Object -FilePath $buildLogPath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed. Showing first matching error lines:" -ForegroundColor Red
        $errorLines = Select-String -Path $buildLogPath -Pattern 'error:|fatal error|undefined reference|No such file|cannot find|Permission denied' -CaseSensitive:$false |
            Select-Object -First 8
        if ($errorLines) {
            $errorLines | ForEach-Object { Write-Host $_.Line -ForegroundColor Red }
        }
        else {
            Write-Host "No compiler error text found; see full log at $buildLogPath" -ForegroundColor Yellow
        }
        throw "Build failed. Full log: $buildLogPath"
    }

    $exePath = Join-Path $BuildDir "WinTools.exe"
    if (-not (Test-Path $exePath)) {
        throw "Executable not found: $exePath"
    }

    Write-Host "Launching $exePath" -ForegroundColor Green
    Start-Process $exePath
}
finally {
    Pop-Location
}
