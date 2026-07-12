$ErrorActionPreference = 'Stop'

$sourcePath = Join-Path (Split-Path -Parent $PSScriptRoot) 'native_capture\selection_overlay.cpp'
$source = Get-Content -Raw -LiteralPath $sourcePath

$failures = @()
if ($source -match 'InvalidateRect\(hwnd,\s*nullptr,\s*TRUE\)') {
    $failures += 'mouse updates still erase the full window background'
}
if ($source -notmatch 'case\s+WM_ERASEBKGND\s*:') {
    $failures += 'WM_ERASEBKGND is not suppressed'
}
if ($source -notmatch 'CreateCompatibleBitmap' -or $source -notmatch 'BitBlt\(dc,\s*0,\s*0') {
    $failures += 'selection frame is not composed offscreen and presented in one blit'
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Error "FAIL: $failure"
    }
    exit 1
}

Write-Output 'PASS: selection overlay uses non-erasing invalidation and buffered presentation.'
