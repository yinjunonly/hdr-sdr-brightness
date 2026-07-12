param(
    [Parameter(Mandatory = $true)]
    [string]$EditorPath
)

$ErrorActionPreference = 'Stop'
$fixture = Join-Path $env:TEMP "hdr-sdr-native-region-$PID.bmp"
$output = [IO.Path]::ChangeExtension($fixture, '.png')
$process = $null

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativeRegionTestWindow {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr value);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr hwnd, uint attribute, out Rect rect, int size);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, System.Text.StringBuilder text, int count);
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
    public static string Title(IntPtr hwnd) { var text = new System.Text.StringBuilder(256); GetWindowText(hwnd, text, text.Capacity); return text.ToString(); }
}
'@

$bitmap = [System.Drawing.Bitmap]::new(800, 600, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
    for ($y = 0; $y -lt $bitmap.Height; $y++) {
        for ($x = 0; $x -lt $bitmap.Width; $x++) {
            $bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(
                255, ($x * 3 + $y) % 256, ($x + $y * 5) % 256, ($x * 7 + $y * 2) % 256))
        }
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
    for ($attempt = 0; $attempt -lt 50 -and $window -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 50
        $window = [NativeRegionTestWindow]::Find([uint32]$process.Id)
    }
    if ($window -eq [IntPtr]::Zero) { throw 'FAIL: native region window was not shown.' }

    $bounds = New-Object NativeRegionTestWindow+Rect
    [NativeRegionTestWindow]::DwmGetWindowAttribute(
        $window, 9, [ref]$bounds, [Runtime.InteropServices.Marshal]::SizeOf($bounds)) | Out-Null
    $windowWidth = $bounds.Right - $bounds.Left
    $windowHeight = $bounds.Bottom - $bounds.Top
    if ($windowWidth -ne 800 -or $windowHeight -ne 600) {
        $title = [NativeRegionTestWindow]::Title($window)
        throw "FAIL: select mode '$title' is ${windowWidth}x${windowHeight}, not an 800x600 physical-pixel overlay."
    }

    $startX = 40; $startY = 60; $endX = 700; $endY = 450
    $messageScale = 96.0 / [NativeRegionTestWindow]::GetDpiForWindow($window)
    $rawStartX = [int][Math]::Round($startX * $messageScale)
    $rawStartY = [int][Math]::Round($startY * $messageScale)
    $rawEndX = [int][Math]::Round($endX * $messageScale)
    $rawEndY = [int][Math]::Round($endY * $messageScale)
    [NativeRegionTestWindow]::PostMessage($window, 0x0201, [IntPtr]::new(1), [NativeRegionTestWindow]::Point($rawStartX, $rawStartY)) | Out-Null
    [NativeRegionTestWindow]::PostMessage($window, 0x0200, [IntPtr]::new(1), [NativeRegionTestWindow]::Point($rawEndX, $rawEndY)) | Out-Null
    [NativeRegionTestWindow]::PostMessage($window, 0x0202, [IntPtr]::Zero, [NativeRegionTestWindow]::Point($rawEndX, $rawEndY)) | Out-Null
    Start-Sleep -Milliseconds 150

    $copyX = 12 + 778
    $copyY = 450 + 10 + 31
    $rawCopyX = [int][Math]::Round($copyX * $messageScale)
    $rawCopyY = [int][Math]::Round($copyY * $messageScale)
    [NativeRegionTestWindow]::PostMessage($window, 0x0201, [IntPtr]::new(1), [NativeRegionTestWindow]::Point($rawCopyX, $rawCopyY)) | Out-Null
    [NativeRegionTestWindow]::PostMessage($window, 0x0202, [IntPtr]::Zero, [NativeRegionTestWindow]::Point($rawCopyX, $rawCopyY)) | Out-Null

    if (-not $process.WaitForExit(5000)) {
        $process.Refresh()
        throw "FAIL: Copy did not close the native region editor (window '$($process.MainWindowTitle)')."
    }
    $clipboard = $null
    for ($attempt = 0; $attempt -lt 30 -and -not $clipboard; $attempt++) {
        try { $clipboard = [System.Windows.Forms.Clipboard]::GetImage() } catch { }
        if (-not $clipboard) { Start-Sleep -Milliseconds 100 }
    }
    if (-not $clipboard) { throw 'FAIL: native region Copy produced no bitmap.' }
    try {
        if ($clipboard.Width -ne 660 -or $clipboard.Height -ne 390) {
            throw "FAIL: copied region is $($clipboard.Width)x$($clipboard.Height), expected 660x390."
        }
        $source = [System.Drawing.Bitmap]::FromFile($fixture)
        try {
            $samples = @(
                @(0, 0),
                @(659, 0),
                @(0, 389),
                @(659, 389)
            )
            foreach ($sample in $samples) {
                $x = [int]$sample[0]
                $y = [int]$sample[1]
                $expected = $source.GetPixel($startX + $x, $startY + $y)
                $actual = ([System.Drawing.Bitmap]$clipboard).GetPixel($x, $y)
                $delta = [Math]::Abs($expected.R - $actual.R) +
                    [Math]::Abs($expected.G - $actual.G) +
                    [Math]::Abs($expected.B - $actual.B)
                if ($delta -gt 3) { throw "FAIL: copied edge pixel $x,$y differs by $delta." }
            }
        } finally {
            $source.Dispose()
        }
    } finally {
        $clipboard.Dispose()
    }
    Write-Output 'PASS: native physical-pixel region selection copied 660x390 with aligned edge pixels.'
} finally {
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    Remove-Item -LiteralPath $fixture,$output -Force -ErrorAction SilentlyContinue
}
