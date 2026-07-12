param(
    [string]$OutputDir
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root 'obj\native-capture'
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

$gpp = Resolve-BuildTool -Name 'g++' -FallbackPaths @(
    'C:\msys64\mingw64\bin\g++.exe',
    'C:\msys64\ucrt64\bin\g++.exe'
)
$toolDir = Split-Path -Parent $gpp
if ($toolDir -and ($env:PATH -notlike "*$toolDir*")) {
    $env:PATH = $toolDir + [IO.Path]::PathSeparator + $env:PATH
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$sources = @(
    'native_capture.cpp',
    'capture_options.cpp',
    'capture_bitmap.cpp',
    'capture_pipeline.cpp',
    'bmp_output.cpp',
    'clipboard_output.cpp',
    'native_capture_backend.cpp',
    'native_pipe_server.cpp',
    'selection_overlay.cpp',
    'tone_map.cpp'
) | ForEach-Object { Join-Path $PSScriptRoot $_ }
$exe = Join-Path $OutputDir 'HdrSdrNativeCapture.exe'

& $gpp $sources `
    -std=gnu++17 `
    -DWIN32_LEAN_AND_MEAN `
    -DNOMINMAX `
    -D_WIN32_WINNT=0x0A00 `
    -O2 `
    -ffunction-sections `
    -fdata-sections `
    -Wall `
    -Wextra `
    -Wno-missing-field-initializers `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    '-Wl,--gc-sections' `
    -s `
    -o $exe `
    -lruntimeobject `
    -lole32 `
    -luser32 `
    -lgdi32 `
    -lmsimg32 `
    -lshell32 `
    -ld3d11 `
    -ldxgi

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Built $exe"
