$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$gpp = @(
    'C:\msys64\mingw64\bin\g++.exe',
    'C:\msys64\ucrt64\bin\g++.exe'
) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $gpp) { throw 'Missing g++.' }

$env:PATH = (Split-Path -Parent $gpp) + [IO.Path]::PathSeparator + $env:PATH
$outputDir = Join-Path $root 'obj\brightness-tests'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$policyExe = Join-Path $outputDir 'brightness_initialization_test.exe'

& $gpp `
    (Join-Path $PSScriptRoot 'brightness_initialization_test.cpp') `
    (Join-Path $root 'src\brightness_initialization.cpp') `
    -std=gnu++17 `
    -O2 `
    -Wall `
    -Wextra `
    -I (Join-Path $root 'src') `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -o $policyExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $policyExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$conversionExe = Join-Path $outputDir 'sdr_brightness_conversion_test.exe'
& $gpp `
    (Join-Path $PSScriptRoot 'sdr_brightness_conversion_test.cpp') `
    (Join-Path $root 'src\display_brightness.cpp') `
    -std=gnu++17 `
    -O2 `
    -Wall `
    -Wextra `
    -I (Join-Path $root 'src') `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -luser32 `
    -o $conversionExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $conversionExe
exit $LASTEXITCODE
