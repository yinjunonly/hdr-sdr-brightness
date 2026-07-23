param(
    [switch]$Clean,
    [string]$Version,
    [switch]$Store,
    [string]$OutputDir,
    [string]$MtPath
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$bin = if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    Join-Path $root 'bin'
} elseif ([IO.Path]::IsPathRooted($OutputDir)) {
    [IO.Path]::GetFullPath($OutputDir)
} else {
    [IO.Path]::GetFullPath((Join-Path $root $OutputDir))
}
$sharedObj = Join-Path $root 'obj'
$obj = if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $sharedObj
} else {
    $outputKey = [IO.Path]::GetFullPath($bin).ToLowerInvariant()
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $hashBytes = $sha256.ComputeHash([Text.Encoding]::UTF8.GetBytes($outputKey))
        $hash = -join ($hashBytes[0..7] | ForEach-Object { $_.ToString('x2') })
        Join-Path $sharedObj (Join-Path 'build' $hash)
    } finally {
        $sha256.Dispose()
    }
}
$sources = @(
    (Join-Path $root 'src\main.cpp'),
    (Join-Path $root 'src\app_hotkeys.cpp'),
    (Join-Path $root 'src\brightness_initialization.cpp'),
    (Join-Path $root 'src\capture_pipe.cpp'),
    (Join-Path $root 'src\capture_paths.cpp'),
    (Join-Path $root 'src\capture_request_queue.cpp'),
    (Join-Path $root 'src\display_brightness.cpp'),
    (Join-Path $root 'src\editor_window_control.cpp'),
    (Join-Path $root 'src\fullscreen_capture_adapter.cpp'),
    (Join-Path $root 'src\hdr_preview.cpp'),
    (Join-Path $root 'src\launch_mode.cpp'),
    (Join-Path $root 'src\night_mode.cpp'),
    (Join-Path $root 'src\process_util.cpp'),
    (Join-Path $root 'src\registry_util.cpp'),
    (Join-Path $root 'src\registry_watcher.cpp'),
    (Join-Path $root 'src\startup_integration.cpp'),
    (Join-Path $root 'src\store_startup_policy.cpp'),
    (Join-Path $root 'src\supporter_code.cpp'),
    (Join-Path $root 'src\tray_icon.cpp'),
    (Join-Path $root 'src\ui_backbuffer.cpp'),
    (Join-Path $root 'src\ui_dpi.cpp'),
    (Join-Path $root 'src\ui_gdiplus.cpp'),
    (Join-Path $root 'src\ui_theme.cpp'),
    (Join-Path $root 'src\ui_window.cpp'),
    (Join-Path $root 'src\localization.cpp')
)
$res = Join-Path $root 'res\app.rc'
$resObj = Join-Path $obj 'app.res.o'
$versionFile = Join-Path $root 'VERSION'
$versionHeader = Join-Path $obj 'version.h'
$manifestTemplate = Join-Path $root 'res\app.manifest.in'
$manifest = Join-Path $obj 'app.manifest'
$exe = Join-Path $bin 'HdrSdrBrightness.exe'
$captureOut = Join-Path $bin 'capture'
$nativeCaptureBuild = Join-Path $root 'native_capture\build.ps1'
$nativeEditorBuild = Join-Path $root 'native_editor\build.ps1'

function Get-ProjectVersion {
    param(
        [string]$Root,
        [string]$RequestedVersion
    )

    if ([string]::IsNullOrWhiteSpace($RequestedVersion)) {
        $path = Join-Path $Root 'VERSION'
        if (-not (Test-Path -LiteralPath $path)) {
            throw "Missing version file: $path"
        }
        $RequestedVersion = (Get-Content -LiteralPath $path -Raw).Trim()
    }

    if ($RequestedVersion -notmatch '^\d+\.\d+\.\d+$') {
        throw "Version must use MAJOR.MINOR.PATCH, for example 1.0.0"
    }

    return $RequestedVersion
}

function Convert-ToResourceVersion {
    param([string]$Version)

    $parts = @($Version.Split('.')) + @('0')
    foreach ($part in $parts) {
        $value = [int]$part
        if ($value -lt 0 -or $value -gt 65535) {
            throw "Version part out of range for Windows resources: $part"
        }
    }

    return $parts
}

function Resolve-BuildTool {
    param(
        [string]$Name,
        [string[]]$FallbackPaths
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    foreach ($path in $FallbackPaths) {
        if (Test-Path -LiteralPath $path) {
            return $path
        }
    }

    throw "Missing build tool: $Name"
}

function Resolve-WindowsSdkTool {
    param(
        [string]$Name,
        [string]$RequestedPath
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
        if (-not (Test-Path -LiteralPath $RequestedPath)) {
            throw "Missing Windows SDK tool: $RequestedPath"
        }
        return (Resolve-Path -LiteralPath $RequestedPath).Path
    }

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $kitBin = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
    if (Test-Path -LiteralPath $kitBin) {
        $matches = Get-ChildItem -LiteralPath $kitBin -Recurse -Filter $Name -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\x64\\' } |
            Sort-Object FullName -Descending
        if ($matches) {
            return $matches[0].FullName
        }
    }

    throw "Missing Windows SDK tool: $Name. Install the Windows SDK or pass -MtPath."
}

$Version = Get-ProjectVersion -Root $root -RequestedVersion $Version
$versionParts = Convert-ToResourceVersion -Version $Version
$version4 = $versionParts -join '.'

if ($Clean -and (Test-Path $bin)) {
    $resolvedBin = (Resolve-Path -LiteralPath $bin).Path
    if (-not $resolvedBin.StartsWith($root + '\')) {
        throw "Refusing to clean outside project: $resolvedBin"
    }
    Remove-Item -LiteralPath $resolvedBin -Recurse -Force
}
if ($Clean -and (Test-Path $obj)) {
    $resolvedObj = (Resolve-Path -LiteralPath $obj).Path
    if (-not $resolvedObj.StartsWith($root + '\')) {
        throw "Refusing to clean outside project: $resolvedObj"
    }
    Remove-Item -LiteralPath $resolvedObj -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $bin | Out-Null
New-Item -ItemType Directory -Force -Path $obj | Out-Null

if (-not (Test-Path -LiteralPath $manifestTemplate)) {
    throw "Missing manifest template: $manifestTemplate"
}

$gpp = Resolve-BuildTool -Name 'g++' -FallbackPaths @(
    'C:\msys64\mingw64\bin\g++.exe',
    'C:\msys64\ucrt64\bin\g++.exe'
)
$windres = Resolve-BuildTool -Name 'windres' -FallbackPaths @(
    'C:\msys64\mingw64\bin\windres.exe',
    'C:\msys64\ucrt64\bin\windres.exe'
)
$mt = Resolve-WindowsSdkTool -Name 'mt.exe' -RequestedPath $MtPath
$toolDir = Split-Path -Parent $gpp
if ($toolDir -and ($env:PATH -notlike "*$toolDir*")) {
    $env:PATH = $toolDir + [IO.Path]::PathSeparator + $env:PATH
}

$icon = Join-Path $root 'assets\app.ico'
if (-not (Test-Path -LiteralPath $icon)) {
    throw "Missing icon: $icon"
}

Push-Location $root
try {
$versionHeaderContent = @"
#pragma once
#define APP_VERSION "$Version"
#define APP_VERSION_W L"$Version"
#define APP_VERSION_COMMA $($versionParts -join ',')
"@
    Set-Content -LiteralPath $versionHeader -Value $versionHeaderContent -Encoding ASCII

    $manifestContent = (Get-Content -LiteralPath $manifestTemplate -Raw).Replace('@VERSION4@', $version4)
    Set-Content -LiteralPath $manifest -Value $manifestContent -Encoding UTF8

    & $windres --codepage=65001 -I $root -I $obj $res -O coff -o $resObj
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & $gpp $sources $resObj `
        -std=gnu++11 `
        -O2 `
        -ffunction-sections `
        -fdata-sections `
        -mwindows `
        -I $obj `
        -DUNICODE `
        -D_UNICODE `
        $(if ($Store) { '-DHSB_STORE_BUILD=1' } else { @() }) `
        -Wall `
        -Wextra `
        -Wno-missing-field-initializers `
        -static `
        -static-libgcc `
        -static-libstdc++ `
        '-Wl,--gc-sections' `
        -s `
        -o $exe `
        -luser32 `
        -lshell32 `
        -lgdi32 `
        -lgdiplus `
        -ladvapi32

    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & $mt -nologo -manifest $manifest "-outputresource:$exe;#1"
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    if (Test-Path -LiteralPath $nativeCaptureBuild) {
        & powershell -ExecutionPolicy Bypass -File $nativeCaptureBuild -OutputDir $captureOut
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    if (Test-Path -LiteralPath $nativeEditorBuild) {
        & powershell -ExecutionPolicy Bypass -File $nativeEditorBuild -OutputDir $captureOut -Version $Version
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
} finally {
    Pop-Location
}

$flavor = if ($Store) { 'Store' } else { 'desktop' }
Write-Host "Built $exe (version $Version, $flavor)"
