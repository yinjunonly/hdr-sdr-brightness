param(
    [Parameter(Mandatory = $true)]
    [string]$BuildRoot
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$gpp = @(
    'C:\msys64\mingw64\bin\g++.exe',
    'C:\msys64\ucrt64\bin\g++.exe'
) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $gpp) { throw 'Missing g++.' }
$toolDir = Split-Path -Parent $gpp
$env:PATH = $toolDir + [IO.Path]::PathSeparator + $env:PATH
$exe = Join-Path $BuildRoot 'capture_adapter_test.exe'
try {
    & $gpp `
        (Join-Path $PSScriptRoot 'capture_adapter_test.cpp') `
        (Join-Path $root 'src\fullscreen_capture_adapter.cpp') `
        (Join-Path $root 'src\process_util.cpp') `
        -std=gnu++17 `
        -DUNICODE `
        -D_UNICODE `
        -O2 `
        -static `
        -static-libgcc `
        -static-libstdc++ `
        -o $exe `
        -luser32 `
        -lshell32
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $exe
    exit $LASTEXITCODE
} finally {
    Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue
}
