$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$processUtil = Get-Content -Raw -LiteralPath (Join-Path $root 'src\process_util.cpp')
$main = Get-Content -Raw -LiteralPath (Join-Path $root 'src\main.cpp')

if ($processUtil -notmatch 'LaunchDetachedHidden[\s\S]*?CreateProcessW\([\s\S]*?CREATE_NO_WINDOW') {
    throw 'FAIL: hidden detached launch does not suppress console-window creation.'
}

$nativeServer = [regex]::Match($main, 'void StartNativeCaptureHelperServer\(\)[\s\S]*?^}', 'Multiline').Value
$editorWarmup = [regex]::Match($main, 'void StartNativeEditorWarmup\(\)[\s\S]*?^}', 'Multiline').Value
if ($nativeServer -notmatch 'LaunchDetachedHidden' -or $editorWarmup -notmatch 'LaunchDetachedHidden') {
    throw 'FAIL: native capture warmup or editor warmup still uses a visible launcher.'
}

Write-Output 'PASS: native capture and editor warmups launch without a console window.'
