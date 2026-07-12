$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$gpp = @(
    'C:\msys64\mingw64\bin\g++.exe',
    'C:\msys64\ucrt64\bin\g++.exe'
) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $gpp) {
    throw 'Missing g++.'
}

$env:PATH = (Split-Path -Parent $gpp) + [IO.Path]::PathSeparator + $env:PATH
$out = Join-Path $PSScriptRoot 'capture_bitmap_test.exe'
& $gpp (Join-Path $PSScriptRoot 'capture_bitmap_test.cpp') `
    (Join-Path $root 'native_capture\capture_bitmap.cpp') `
    -std=gnu++17 `
    -O2 `
    -I (Join-Path $root 'native_capture') `
    -o $out
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $out
exit $LASTEXITCODE
