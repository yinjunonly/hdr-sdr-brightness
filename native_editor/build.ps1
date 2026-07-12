param(
    [string]$OutputDir,
    [string]$Version
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root 'obj\native-editor'
}

function Resolve-BuildTool {
    param([string]$Name, [string[]]$FallbackPaths)
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    foreach ($path in $FallbackPaths) {
        if (Test-Path -LiteralPath $path) { return $path }
    }
    throw "Missing build tool: $Name"
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
}
if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    throw "Version must use MAJOR.MINOR.PATCH: $Version"
}
$versionParts = @($Version.Split('.')) + @('0')
$version4 = $versionParts -join '.'

$gpp = Resolve-BuildTool -Name 'g++' -FallbackPaths @(
    'C:\msys64\mingw64\bin\g++.exe',
    'C:\msys64\ucrt64\bin\g++.exe'
)
$windres = Resolve-BuildTool -Name 'windres' -FallbackPaths @(
    'C:\msys64\mingw64\bin\windres.exe',
    'C:\msys64\ucrt64\bin\windres.exe'
)
$toolDir = Split-Path -Parent $gpp
if ($toolDir -and ($env:PATH -notlike "*$toolDir*")) {
    $env:PATH = $toolDir + [IO.Path]::PathSeparator + $env:PATH
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$resourceHasher = [Security.Cryptography.SHA256]::Create()
try {
    $resourceHash = $resourceHasher.ComputeHash(
        [Text.Encoding]::UTF8.GetBytes(([IO.Path]::GetFullPath($OutputDir) + '|' + $Version)))
    $resourceKey = -join ($resourceHash[0..7] | ForEach-Object { $_.ToString('x2') })
} finally {
    $resourceHasher.Dispose()
}
$resourceDir = Join-Path $root (Join-Path 'obj\native-editor-resource' $resourceKey)
New-Item -ItemType Directory -Force -Path $resourceDir | Out-Null
$versionHeader = Join-Path $resourceDir 'editor_version.h'
$manifest = Join-Path $resourceDir 'editor.manifest'
$resourceScript = Join-Path $resourceDir 'editor.rc'
$resourceObject = Join-Path $resourceDir 'editor.res.o'
$icon = Join-Path $root 'assets\app.ico'
if (-not (Test-Path -LiteralPath $icon)) { throw "Missing icon: $icon" }

Set-Content -LiteralPath $versionHeader -Encoding ASCII -Value @"
#pragma once
#define EDITOR_VERSION "$Version"
#define EDITOR_VERSION_COMMA $($versionParts -join ',')
"@
Set-Content -LiteralPath $manifest -Encoding UTF8 -Value @"
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity version="$version4" processorArchitecture="*" name="HdrSdrNativeEditor" type="win32"/>
  <dependency><dependentAssembly><assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0" processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*"/></dependentAssembly></dependency>
  <application xmlns="urn:schemas-microsoft-com:asm.v3"><windowsSettings>
    <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true</dpiAware>
    <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2, PerMonitor</dpiAwareness>
  </windowsSettings></application>
</assembly>
"@
$iconRc = $icon.Replace('\', '/')
$manifestRc = $manifest.Replace('\', '/')
Set-Content -LiteralPath $resourceScript -Encoding ASCII -Value @"
#include <winver.h>
#include "editor_version.h"
#define IDI_EDITOR_ICON 101
IDI_EDITOR_ICON ICON "$iconRc"
1 RT_MANIFEST "$manifestRc"
VS_VERSION_INFO VERSIONINFO
 FILEVERSION EDITOR_VERSION_COMMA
 PRODUCTVERSION EDITOR_VERSION_COMMA
 FILEOS 0x40004L
 FILETYPE 0x1L
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904B0"
    BEGIN
      VALUE "CompanyName", "HDR SDR Brightness"
      VALUE "FileDescription", "HDR SDR Native Screenshot Editor"
      VALUE "FileVersion", EDITOR_VERSION
      VALUE "InternalName", "HdrSdrNativeEditor"
      VALUE "OriginalFilename", "HdrSdrNativeEditor.exe"
      VALUE "ProductName", "HDR SDR Brightness"
      VALUE "ProductVersion", EDITOR_VERSION
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x0409, 1200
  END
END
"@
& $windres --codepage=65001 -I $resourceDir $resourceScript -O coff -o $resourceObject
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$sources = @(
    'native_editor.cpp',
    'editor_options.cpp',
    'editor_single_instance.cpp',
    'editor_window.cpp',
    'preview_editor.cpp',
    'preview_viewport.cpp',
    'region_selection.cpp',
    'editor_toolbar.cpp',
    'editor_icons.cpp',
    'editor_text.cpp',
    'editor_tooltips.cpp',
    'editor_clipboard.cpp',
    'dib_surface.cpp',
    'image_document.cpp',
    'annotation_renderer.cpp',
    'mosaic_renderer.cpp',
    'bmp_codec.cpp',
    'wic_png.cpp'
) | ForEach-Object { Join-Path $PSScriptRoot $_ }
$exe = Join-Path $OutputDir 'HdrSdrNativeEditor.exe'

& $gpp $sources $resourceObject `
    -std=gnu++17 `
    -DWIN32_LEAN_AND_MEAN `
    -DNOMINMAX `
    -D_WIN32_WINNT=0x0A00 `
    -DUNICODE `
    -D_UNICODE `
    -O2 `
    -ffunction-sections `
    -fdata-sections `
    -mwindows `
    -Wall `
    -Wextra `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    '-Wl,--gc-sections' `
    -s `
    -o $exe `
    -luser32 `
    -lgdi32 `
    -lgdiplus `
    -ldwmapi `
    -lole32 `
    -lshell32 `
    -lcomdlg32 `
    -lcomctl32 `
    -lwindowscodecs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Built $exe"
