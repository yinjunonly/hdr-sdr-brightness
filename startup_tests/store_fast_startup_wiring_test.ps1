$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$startup = Get-Content -Raw -LiteralPath (Join-Path $root 'src\startup_integration.cpp')
$startupHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\startup_integration.h')
$main = Get-Content -Raw -LiteralPath (Join-Path $root 'src\main.cpp')
$launchMode = Get-Content -Raw -LiteralPath (Join-Path $root 'src\launch_mode.cpp')
$build = Get-Content -Raw -LiteralPath (Join-Path $root 'build.ps1')

if ($startup -notmatch 'HdrSdrBrightnessStoreFastStartup' -or
    $startup -notmatch 'HdrSdrBrightnessStore\.exe' -or
    $startup -notmatch '--store-fast-startup') {
    throw 'FAIL: Store fast startup task is not bound to the stable alias and distinct launch mode.'
}
if ($startup -notmatch 'store_startup_policy::SetEnabled' -or
    $startup -notmatch 'store_startup_policy::Reconcile' -or
    $startup -notmatch 'store_startup_policy::ShouldRunBackground') {
    throw 'FAIL: production Store startup integration does not use the tested single-toggle policy.'
}
if ($startupHeader -notmatch 'RepairStoreFastStartupIfNeeded' -or
    $startupHeader -notmatch 'ShouldRunStoreFastStartup') {
    throw 'FAIL: Store startup repair or launch guard is missing from the public integration interface.'
}
if ($launchMode -notmatch '--store-fast-startup') {
    throw 'FAIL: launch-mode parser does not recognize the Store fast-startup invocation.'
}
if ($main -notmatch 'IsStoreFastStartupLaunch\(\)[\s\S]*?ShouldRunStoreFastStartup\(\)[\s\S]*?return 0;') {
    throw 'FAIL: Store fast-startup launch is not rejected when Windows startup is disabled.'
}
if ($main -notmatch 'RepairStoreFastStartupIfNeeded') {
    throw 'FAIL: existing Store users will not have a missing fast-startup task repaired after upgrade.'
}
if ($build -notmatch 'src\\store_startup_policy\.cpp') {
    throw 'FAIL: Store startup policy is not included in production builds.'
}

Write-Output 'PASS: Store fast startup policy is wired into the production launch path.'
