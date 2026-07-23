$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$main = Get-Content -LiteralPath (Join-Path $root 'src\main.cpp') -Raw
$build = Get-Content -LiteralPath (Join-Path $root 'build.ps1') -Raw
$verify = Get-Content -LiteralPath (Join-Path $root 'verify.ps1') -Raw

if ($main -notmatch '#include\s+"brightness_initialization\.h"') {
    throw 'main.cpp does not include the first-run brightness initialization policy.'
}
if ($main -notmatch 'BrightnessDefaultsInitialized') {
    throw 'main.cpp does not persist a one-time brightness initialization marker.'
}
if ($main -notmatch 'ReadCurrentSdrBrightness') {
    throw 'main.cpp does not read the current Windows SDR brightness for first-run initialization.'
}
if ($main -notmatch 'brightness_initialization::Resolve') {
    throw 'main.cpp does not apply the tested first-run initialization policy.'
}
if ($main -notmatch 'if\s*\(\s*!g_brightnessConfigReady\s*\)\s*\{\s*StopBrightnessTransition\(\);\s*return;\s*\}') {
    throw 'ApplyCurrentBrightness does not wait safely when current brightness initialization fails.'
}
if ($build -notmatch 'src\\brightness_initialization\.cpp') {
    throw 'build.ps1 does not compile the brightness initialization policy.'
}
if ($verify -notmatch 'brightness_tests\\build\.ps1') {
    throw 'verify.ps1 does not run the first-run brightness regression tests.'
}

Write-Host 'PASS: first-run brightness initialization is wired into production.'
