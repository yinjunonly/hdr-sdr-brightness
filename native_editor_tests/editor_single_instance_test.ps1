param(
    [Parameter(Mandatory = $true)]
    [string]$EditorPath
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativeEditorSingleInstanceApi {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr value);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindowsProc callback, IntPtr value);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd, System.Text.StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    public static IntPtr Find(uint targetProcessId) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((hwnd, _) => {
            uint processId;
            GetWindowThreadProcessId(hwnd, out processId);
            var className = new System.Text.StringBuilder(128);
            GetClassName(hwnd, className, className.Capacity);
            string value = className.ToString();
            if (processId == targetProcessId && IsWindowVisible(hwnd) &&
                (value == "HdrSdrNativeEditorRegionWindow" ||
                 value == "HdrSdrNativeEditorPreviewWindow")) {
                result = hwnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
'@

$fixture = Join-Path $env:TEMP "hdr-sdr-native-editor-singleton-$PID.bmp"
$output = "$fixture.png"
$bitmap = [Drawing.Bitmap]::new(640, 480)
try {
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try { $graphics.Clear([Drawing.Color]::FromArgb(25, 70, 110)) }
    finally { $graphics.Dispose() }
    $bitmap.Save($fixture, [Drawing.Imaging.ImageFormat]::Bmp)
} finally {
    $bitmap.Dispose()
}
$processes = @()
try {
    $arguments = @('--edit-file', $fixture, '--output', $output, '--lang', '2', '--skip-initial-copy')
    $processes += Start-Process -FilePath $EditorPath -ArgumentList $arguments -PassThru
    $firstWindow = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 50 -and $firstWindow -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 40
        $firstWindow = [NativeEditorSingleInstanceApi]::Find([uint32]$processes[0].Id)
    }
    if ($firstWindow -eq [IntPtr]::Zero) { throw 'FAIL: first native editor did not open.' }

    $processes += Start-Process -FilePath $EditorPath -ArgumentList $arguments -PassThru
    Start-Sleep -Milliseconds 40
    $processes += Start-Process -FilePath $EditorPath -ArgumentList $arguments -PassThru

    $latestWindow = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 80; $attempt++) {
        Start-Sleep -Milliseconds 50
        $latestWindow = [NativeEditorSingleInstanceApi]::Find([uint32]$processes[2].Id)
        $olderExited = $processes[0].HasExited -and $processes[1].HasExited
        if ($olderExited -and $latestWindow -ne [IntPtr]::Zero) { break }
    }

    $visible = 0
    foreach ($process in $processes) {
        if (-not $process.HasExited -and
            [NativeEditorSingleInstanceApi]::Find([uint32]$process.Id) -ne [IntPtr]::Zero) {
            $visible++
        }
    }
    if (-not $processes[0].HasExited -or -not $processes[1].HasExited -or
        $latestWindow -eq [IntPtr]::Zero -or $visible -ne 1) {
        throw "FAIL: latest editor did not replace older windows (visible=$visible)."
    }

    [NativeEditorSingleInstanceApi]::PostMessage(
        $latestWindow, 0x0100, [IntPtr]::new(0x1B), [IntPtr]::Zero) | Out-Null
    if (-not $processes[2].WaitForExit(3000)) { throw 'FAIL: latest editor did not close.' }
    Write-Output 'PASS: rapid editor launches keep only the latest screenshot window.'
} finally {
    foreach ($process in $processes) {
        if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    }
    Remove-Item -LiteralPath $fixture,$output -Force -ErrorAction SilentlyContinue
}
