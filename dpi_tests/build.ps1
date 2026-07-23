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
$outputDir = Join-Path $root 'obj\dpi-tests'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$exe = Join-Path $outputDir 'dpi_awareness_test.exe'

& $gpp `
    (Join-Path $PSScriptRoot 'dpi_awareness_test.cpp') `
    (Join-Path $root 'src\ui_dpi.cpp') `
    -std=gnu++11 `
    -O2 `
    -Wall `
    -Wextra `
    -Wno-cast-function-type `
    -I (Join-Path $root 'src') `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -o $exe `
    -luser32 `
    -lgdi32
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $exe
exit $LASTEXITCODE
