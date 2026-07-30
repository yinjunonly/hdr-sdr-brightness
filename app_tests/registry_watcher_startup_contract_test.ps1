$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -Raw (Join-Path $root 'src\main.cpp')

$startFunction = [regex]::Match(
    $source,
    'void\s+StartRegistryThread\s*\((?<parameters>[^)]*)\)\s*\{(?<body>.*?)\n\}',
    [Text.RegularExpressions.RegexOptions]::Singleline
)
if (-not $startFunction.Success) {
    throw 'Could not find StartRegistryThread.'
}

$parameters = $startFunction.Groups['parameters'].Value
$startBody = $startFunction.Groups['body'].Value
if ($parameters -notmatch '\bHWND\s+notifyWindow\b') {
    throw 'StartRegistryThread must receive the created window handle explicitly.'
}
if ($startBody -notmatch 'registry_watcher::Start\s*\(\s*&g_registryWatcher\s*,\s*notifyWindow\s*,') {
    throw 'StartRegistryThread must pass its explicit window handle to registry_watcher::Start.'
}

$mainWindowProcAt = $source.IndexOf('LRESULT CALLBACK MainWndProc')
$createBranchAt = $source.IndexOf('case WM_CREATE:', $mainWindowProcAt)
$nextBranchAt = $source.IndexOf('case kApplyMessage:', $createBranchAt)
if ($mainWindowProcAt -lt 0 -or $createBranchAt -lt 0 -or $nextBranchAt -lt 0) {
    throw 'Could not find the WM_CREATE branch.'
}
$createBranchBody = $source.Substring(
    $createBranchAt,
    $nextBranchAt - $createBranchAt
)
if ($createBranchBody -notmatch 'StartRegistryThread\s*\(\s*hwnd\s*\)\s*;') {
    throw 'WM_CREATE must start the registry watcher with its valid hwnd parameter.'
}

Write-Host 'PASS: WM_CREATE gives the registry watcher its created window handle.'
