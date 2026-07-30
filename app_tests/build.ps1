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
$outputDir = Join-Path $root 'obj\app-tests'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$exe = Join-Path $outputDir 'night_mode_cache_test.exe'
$watcherExe = Join-Path $outputDir 'registry_watcher_null_target_test.exe'
$storeBridgeExe = Join-Path $outputDir 'store_registry_event_bridge_test.exe'

& $gpp (Join-Path $PSScriptRoot 'night_mode_cache_test.cpp') `
    (Join-Path $root 'src\night_mode.cpp') `
    -std=gnu++17 `
    -O2 `
    -Wall `
    -Wextra `
    -I (Join-Path $root 'src') `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -s `
    -o $exe
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $exe
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $gpp (Join-Path $PSScriptRoot 'registry_watcher_null_target_test.cpp') `
    (Join-Path $root 'src\registry_watcher.cpp') `
    -std=gnu++17 `
    -O2 `
    -Wall `
    -Wextra `
    -I (Join-Path $root 'src') `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -s `
    -o $watcherExe
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $watcherExe
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $gpp (Join-Path $PSScriptRoot 'store_registry_event_bridge_test.cpp') `
    (Join-Path $root 'src\store_registry_event_bridge.cpp') `
    -std=gnu++17 `
    -O2 `
    -Wall `
    -Wextra `
    -I (Join-Path $root 'src') `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -s `
    -o $storeBridgeExe `
    -lole32 `
    -loleaut32 `
    -lwbemuuid `
    -ladvapi32
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $storeBridgeExe
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& (Join-Path $PSScriptRoot 'registry_watcher_startup_contract_test.ps1')
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& (Join-Path $PSScriptRoot 'periodic_night_refresh_contract_test.ps1')
exit $LASTEXITCODE
