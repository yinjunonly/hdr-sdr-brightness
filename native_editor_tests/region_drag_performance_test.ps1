param(
    [Parameter(Mandatory = $true)]
    [string]$EditorPath,
    [int]$FrameCount = 120,
    [double]$MinimumFramesPerSecond = 55.0
)

$ErrorActionPreference = 'Stop'
$fixture = Join-Path $env:TEMP "hdr-sdr-native-drag-perf-$PID.bmp"
$output = [IO.Path]::ChangeExtension($fixture, '.png')
$process = $null

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativeDragPerfWindow {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr value);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr value);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool UpdateWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    public static IntPtr Find(uint processId) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((hwnd, _) => {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner == processId && IsWindowVisible(hwnd)) { result = hwnd; return false; }
            return true;
        }, IntPtr.Zero);
        return result;
    }
    public static IntPtr Point(int x, int y) { return new IntPtr((y << 16) | (x & 0xffff)); }
}
'@

$bitmap = [System.Drawing.Bitmap]::new(3440, 1440, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $brush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
            [System.Drawing.Rectangle]::new(0, 0, 3440, 1440),
            [System.Drawing.Color]::FromArgb(255, 20, 42, 76),
            [System.Drawing.Color]::FromArgb(255, 218, 132, 62),
            20.0)
        try { $graphics.FillRectangle($brush, 0, 0, 3440, 1440) } finally { $brush.Dispose() }
    } finally {
        $graphics.Dispose()
    }
    $bitmap.Save($fixture, [System.Drawing.Imaging.ImageFormat]::Bmp)
} finally {
    $bitmap.Dispose()
}

try {
    $process = Start-Process -FilePath $EditorPath -ArgumentList @(
        '--select-file', $fixture, '--output', $output, '--lang', '2'
    ) -PassThru
    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 80 -and $window -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 25
        $window = [NativeDragPerfWindow]::Find([uint32]$process.Id)
    }
    if ($window -eq [IntPtr]::Zero) { throw 'FAIL: native region window was not shown.' }

    $scale = 96.0 / [NativeDragPerfWindow]::GetDpiForWindow($window)
    function Message-Point([int]$x, [int]$y) {
        $rawX = [int][Math]::Round($x * $scale)
        $rawY = [int][Math]::Round($y * $scale)
        return [NativeDragPerfWindow]::Point($rawX, $rawY)
    }

    [NativeDragPerfWindow]::SendMessage(
        $window, 0x0201, [IntPtr]::new(1), (Message-Point 180 180)) | Out-Null
    $watch = [Diagnostics.Stopwatch]::StartNew()
    for ($frame = 1; $frame -le $FrameCount; $frame++) {
        $x = 180 + [int][Math]::Round(2600.0 * $frame / $FrameCount)
        $y = 360 + [int][Math]::Round(180.0 * [Math]::Sin($frame / 8.0))
        [NativeDragPerfWindow]::SendMessage(
            $window, 0x0200, [IntPtr]::new(1), (Message-Point $x $y)) | Out-Null
        [NativeDragPerfWindow]::UpdateWindow($window) | Out-Null
    }
    $watch.Stop()
    $fps = $FrameCount / $watch.Elapsed.TotalSeconds
    [NativeDragPerfWindow]::SendMessage(
        $window, 0x0202, [IntPtr]::Zero, (Message-Point 2780 360)) | Out-Null
    if ($fps -lt $MinimumFramesPerSecond) {
        throw "FAIL: 3440x1440 synchronous region drag rendered at $([Math]::Round($fps, 1)) FPS; minimum is $MinimumFramesPerSecond FPS."
    }
    Write-Output "PASS: 3440x1440 synchronous region drag rendered at $([Math]::Round($fps, 1)) FPS."
} finally {
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    Remove-Item -LiteralPath $fixture,$output -Force -ErrorAction SilentlyContinue
}
