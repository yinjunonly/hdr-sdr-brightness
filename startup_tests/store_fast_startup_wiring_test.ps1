$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$startup = Get-Content -Raw -LiteralPath (Join-Path $root 'src\startup_integration.cpp')
$startupHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\startup_integration.h')
$main = Get-Content -Raw -LiteralPath (Join-Path $root 'src\main.cpp')
$launchMode = Get-Content -Raw -LiteralPath (Join-Path $root 'src\launch_mode.cpp')
$build = Get-Content -Raw -LiteralPath (Join-Path $root 'build.ps1')

$forbidden = @(
    'HdrSdrBrightnessStoreFastStartup',
    'HdrSdrBrightnessStore\.exe',
    '--store-fast-startup',
    'powershell\.exe',
    'ExecutionPolicy',
    'Register-ScheduledTask',
    'New-ScheduledTask',
    'schtasks\.exe /Create'
)
foreach ($pattern in $forbidden) {
    if ($startup -match $pattern -or
        $startupHeader -match $pattern -or
        $launchMode -match $pattern -or
        $main -match $pattern) {
        throw "FAIL: production startup code still contains forbidden persistence behavior: $pattern"
    }
}
if ($startup -notmatch 'store_startup_policy::SetEnabled') {
    throw 'FAIL: Store startup toggle does not use the tested Windows-managed policy.'
}
if ($startupHeader -match 'RepairStoreFastStartupIfNeeded|ShouldRunStoreFastStartup' -or
    $launchMode -match 'IsStoreFastStartupLaunch' -or
    $main -match 'RepairStoreFastStartupIfNeeded|ShouldRunStoreFastStartup|IsStoreFastStartupLaunch') {
    throw 'FAIL: obsolete Store fast-startup interface is still wired into production.'
}
if ($startup -notmatch 'SetPortableStartupEnabled[\s\S]*?SetRunKeyStartupEnabled' -or
    $startup -notmatch 'MigratePortableStartupIfNeeded') {
    throw 'FAIL: portable startup is not using the Run key with legacy task migration.'
}
if ($main -notmatch 'MigratePortableStartupIfNeeded') {
    throw 'FAIL: portable startup does not migrate legacy scheduled tasks after upgrade.'
}
if ($build -notmatch 'src\\store_startup_policy\.cpp') {
    throw 'FAIL: Store startup policy is not included in production builds.'
}

Write-Output 'PASS: Store uses only Windows startup and desktop no longer creates scheduled tasks.'
