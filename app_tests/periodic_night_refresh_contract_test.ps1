$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -Raw (Join-Path $root 'src\main.cpp')
$timerBranch = [regex]::Match(
    $source,
    'if\s*\(wParam\s*==\s*kRecheckTimer\)\s*\{(?<body>.*?)\n\s*\}',
    [Text.RegularExpressions.RegexOptions]::Singleline
)
if (-not $timerBranch.Success) {
    throw 'Could not find the periodic recheck timer branch.'
}

$body = $timerBranch.Groups['body'].Value
$invalidateAt = $body.IndexOf('night_mode::InvalidateActiveStateCache();')
$applyAt = $body.IndexOf('ApplyCurrentBrightness(false);')
if ($invalidateAt -lt 0 -or $applyAt -lt 0 -or $invalidateAt -gt $applyAt) {
    throw 'Periodic recheck must invalidate the cached Night Light active state before applying brightness.'
}

Write-Host 'PASS: periodic recheck refreshes Night Light active state before applying brightness.'
