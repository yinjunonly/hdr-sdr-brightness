param(
    [string]$EditorPath
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$editor = if ($EditorPath) {
    $EditorPath
} else {
    Join-Path $root 'bin\capture\HdrSdrNativeEditor.exe'
}
if (-not (Test-Path -LiteralPath $editor)) {
    throw "FAIL: native editor is not built: $editor"
}

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativeEditorSmokeApi {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    public static IntPtr FindVisibleWindow(uint targetProcessId) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((hwnd, _) => {
            uint processId;
            GetWindowThreadProcessId(hwnd, out processId);
            if (processId == targetProcessId && IsWindowVisible(hwnd)) {
                result = hwnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
'@

$fixture = Join-Path $env:TEMP "hdr-sdr-native-editor-smoke-$PID.bmp"
$output = "$fixture.png"
$bitmap = [System.Drawing.Bitmap]::new(640, 360)
try {
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(30, 60, 90))
    } finally {
        $graphics.Dispose()
    }
    $bitmap.Save($fixture, [System.Drawing.Imaging.ImageFormat]::Bmp)
} finally {
    $bitmap.Dispose()
}

$process = $null
try {
    $process = Start-Process -FilePath $editor -ArgumentList @(
        '--edit-file', $fixture, '--output', $output, '--lang', '2', '--skip-initial-copy'
    ) -PassThru
    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 40 -and $window -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 50
        $window = [NativeEditorSmokeApi]::FindVisibleWindow([uint32]$process.Id)
    }
    if ($window -eq [IntPtr]::Zero) {
        throw 'FAIL: native editor did not expose a visible preview window.'
    }

    [NativeEditorSmokeApi]::PostMessage($window, 0x0100, [IntPtr]::new(0x1B), [IntPtr]::Zero) | Out-Null
    if (-not $process.WaitForExit(3000)) {
        throw 'FAIL: Esc did not close the native editor.'
    }
    if ($process.ExitCode -ne 0) {
        throw "FAIL: native editor exited with code $($process.ExitCode)."
    }
    Write-Output 'PASS: native editor opens a physical preview and closes with Esc.'
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
    }
    Remove-Item -LiteralPath $fixture,$output -Force -ErrorAction SilentlyContinue
}
