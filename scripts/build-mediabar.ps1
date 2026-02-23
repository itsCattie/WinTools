Param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [switch]$Clean,

    [switch]$SkipProcessStop
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$sourceDir = Join-Path $repoRoot "src\modules\MediaBar"
$buildDir = Join-Path $sourceDir "build"

function Find-FirstExistingPath {
    Param(
        [Parameter(Mandatory = $true)]
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Get-QtBinPath {
    $qtCandidates = @(
        "C:\Qt\6.9.3\mingw_64\bin",
        "C:\Qt\6.9.2\mingw_64\bin",
        "C:\Qt\6.8.3\mingw_64\bin",
        "C:\Qt\6.8.2\mingw_64\bin",
        "C:\Qt\6.8.1\mingw_64\bin",
        "C:\Qt\6.8.0\mingw_64\bin",
        "C:\Qt\6.7.3\mingw_64\bin",
        "C:\Qt\6.7.2\mingw_64\bin",
        "C:\Qt\6.7.1\mingw_64\bin",
        "C:\Qt\6.7.0\mingw_64\bin",
        "C:\Qt\6.6.3\mingw_64\bin",
        "C:\Qt\6.6.2\mingw_64\bin",
        "C:\Qt\6.6.1\mingw_64\bin",
        "C:\Qt\6.6.0\mingw_64\bin",
        "C:\Qt\6.5.3\mingw_64\bin",
        "C:\Qt\6.5.2\mingw_64\bin",
        "C:\Qt\6.5.1\mingw_64\bin",
        "C:\Qt\6.5.0\mingw_64\bin"
    )

    $preferred = Find-FirstExistingPath -Candidates $qtCandidates
    if ($preferred) {
        return $preferred
    }

    $commandPath = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if ($commandPath) {
        return Split-Path $commandPath.Source -Parent
    }

    return $null
}

function Get-MinGwBinPath {
    Param(
        [Parameter(Mandatory = $true)]
        [string]$QtBinPath
    )

    $qtVersionRoot = Split-Path (Split-Path $QtBinPath -Parent) -Parent
    $qtRoot = Split-Path $qtVersionRoot -Parent
    $mingwCandidates = @(
        "C:\Qt\Tools\mingw1310_64\bin",
        "C:\Qt\Tools\mingw1300_64\bin",
        "C:\Qt\Tools\mingw1200_64\bin",
        "C:\Qt\Tools\mingw1120_64\bin",
        (Join-Path $qtRoot "Tools\mingw1310_64\bin"),
        (Join-Path $qtRoot "Tools\mingw1300_64\bin"),
        (Join-Path $qtRoot "Tools\mingw1200_64\bin"),
        (Join-Path $qtRoot "Tools\mingw1120_64\bin"),
        "C:\msys64\mingw64\bin"
    )

    $preferred = Find-FirstExistingPath -Candidates $mingwCandidates
    if ($preferred) {
        return $preferred
    }

    $commandPath = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($commandPath) {
        return Split-Path $commandPath.Source -Parent
    }

    return $null
}

function Get-QtCMakePath {
    Param(
        [Parameter(Mandatory = $true)]
        [string]$QtBinPath
    )

    $qtRoot = Split-Path $QtBinPath -Parent
    return Join-Path $qtRoot "lib\cmake"
}

function Prepend-Path {
    Param(
        [Parameter(Mandatory = $true)]
        [string]$PathToAdd
    )

    if ([string]::IsNullOrWhiteSpace($PathToAdd)) {
        return
    }

    $segments = $env:Path -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    if ($segments -contains $PathToAdd) {
        return
    }

    $env:Path = "$PathToAdd;$env:Path"
}

function Invoke-Step {
    Param(
        [Parameter(Mandatory = $true)]
        [string]$Description,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Action
    )

    Write-Host $Description -ForegroundColor Cyan
    & $Action
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

function Stop-LockingMediaBarProcess {
    if ($SkipProcessStop) {
        return
    }

    $candidateExePaths = @(
        (Join-Path $buildDir "MediaBarCpp.exe"),
        (Join-Path $buildDir "$Configuration\MediaBarCpp.exe")
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

    $running = Get-Process -Name "MediaBarCpp" -ErrorAction SilentlyContinue
    foreach ($proc in $running) {
        try {
            $procPath = $proc.Path
            if ([string]::IsNullOrWhiteSpace($procPath)) {
                continue
            }

            if ($candidateExePaths -contains $procPath) {
                Write-Host "Stopping running MediaBar process (PID=$($proc.Id)) to unlock build output..." -ForegroundColor Yellow
                Stop-Process -Id $proc.Id -Force -ErrorAction Stop
            }
        }
        catch {
            Write-Host "Could not inspect/stop process PID=$($proc.Id): $($_.Exception.Message)" -ForegroundColor Yellow
        }
    }
}

function Assert-CompilerWorks {
    Param(
        [Parameter(Mandatory = $true)]
        [string]$CompilerPath
    )

    $tempCpp = Join-Path $buildDir "__sanity.cpp"
    $tempObj = Join-Path $buildDir "__sanity.obj"
    $tempOut = Join-Path $buildDir "__sanity.stdout.log"
    $tempErr = Join-Path $buildDir "__sanity.stderr.log"

    Write-Host "Using compiler: $CompilerPath" -ForegroundColor DarkCyan
    Set-Content -Path $tempCpp -Encoding Ascii -Value "int main() { return 0; }"

    $arguments = @("-c", $tempCpp, "-o", $tempObj)
    $process = Start-Process -FilePath $CompilerPath -ArgumentList $arguments -PassThru -Wait -NoNewWindow -RedirectStandardOutput $tempOut -RedirectStandardError $tempErr
    if ($process.ExitCode -ne 0 -or -not (Test-Path $tempObj)) {
        Write-Host "Compiler stdout:" -ForegroundColor Yellow
        if (Test-Path $tempOut) {
            Get-Content $tempOut -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
        }

        Write-Host "Compiler stderr:" -ForegroundColor Yellow
        if (Test-Path $tempErr) {
            Get-Content $tempErr -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
        }

        throw "C++ compiler sanity check failed. Ensure MinGW is installed and runnable from this shell."
    }

    Remove-Item $tempCpp -Force -ErrorAction SilentlyContinue
    Remove-Item $tempObj -Force -ErrorAction SilentlyContinue
    Remove-Item $tempOut -Force -ErrorAction SilentlyContinue
    Remove-Item $tempErr -Force -ErrorAction SilentlyContinue
}

if (-not (Test-Path $sourceDir)) {
    throw "MediaBar source directory not found: $sourceDir"
}

$qtBinPath = Get-QtBinPath
if (-not $qtBinPath) {
    throw "Qt bin directory not found. Install Qt (mingw_64) or add it to PATH."
}

$mingwBinPath = Get-MinGwBinPath -QtBinPath $qtBinPath
if (-not $mingwBinPath) {
    throw "MinGW bin directory not found. Install Qt MinGW tools or MSYS2 MinGW."
}

$qtCMakePath = Get-QtCMakePath -QtBinPath $qtBinPath
if (-not (Test-Path $qtCMakePath)) {
    throw "Qt CMake directory not found: $qtCMakePath"
}

Prepend-Path -PathToAdd $qtBinPath
Prepend-Path -PathToAdd $mingwBinPath

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item $buildDir -Recurse -Force
}

if (-not (Test-Path $buildDir)) {
    New-Item -Path $buildDir -ItemType Directory | Out-Null
}

Stop-LockingMediaBarProcess

$compiler = Join-Path $mingwBinPath "g++.exe"
if (-not (Test-Path $compiler)) {
    throw "Compiler not found: $compiler"
}

Assert-CompilerWorks -CompilerPath $compiler

$cmakeConfigureArgs = @(
    "-S", $sourceDir,
    "-B", $buildDir,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_CXX_COMPILER=$compiler",
    "-DCMAKE_PREFIX_PATH=$qtCMakePath"
)

Invoke-Step -Description "Configuring MediaBar C++ project..." -Action {
    & cmake @cmakeConfigureArgs
}

Invoke-Step -Description "Building MediaBar C++ project ($Configuration)..." -Action {
    & cmake --build $buildDir --config $Configuration --verbose
}

$exeCandidates = @(
    (Join-Path $buildDir "MediaBarCpp.exe"),
    (Join-Path $buildDir "$Configuration\MediaBarCpp.exe")
)
$exePath = Find-FirstExistingPath -Candidates $exeCandidates
if (-not $exePath) {
    throw "MediaBar executable was not produced by build."
}

$winDeployQt = Join-Path $qtBinPath "windeployqt.exe"
if (-not (Test-Path $winDeployQt)) {
    throw "windeployqt.exe not found in Qt bin path: $qtBinPath"
}

$exeDir = Split-Path $exePath -Parent
Invoke-Step -Description "Deploying Qt runtime with windeployqt..." -Action {
    & $winDeployQt --release --compiler-runtime --dir $exeDir $exePath
}

Write-Host "MediaBar build + Qt deployment completed." -ForegroundColor Green