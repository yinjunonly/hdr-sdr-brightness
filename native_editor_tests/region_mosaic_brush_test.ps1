param(
    [Parameter(Mandatory = $true)]
    [string]$EditorPath,
    [string]$PreviewPath
)

$ErrorActionPreference = 'Stop'
$fixture = Join-Path $env:TEMP "hdr-sdr-native-mosaic-$PID.bmp"
$output = [IO.Path]::ChangeExtension($fixture, '.png')
$process = $null

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativeMosaicBrushWindow {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr value);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr value);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool UpdateWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr hwnd, IntPtr dc);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr destination, uint flags);
    [DllImport("gdi32.dll")] public static extern bool BitBlt(IntPtr destination, int x, int y, int width, int height, IntPtr source, int sourceX, int sourceY, uint operation);
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

function Send-Mouse {
    param([IntPtr]$Window, [uint32]$Message, [int]$X, [int]$Y, [double]$Scale, [bool]$Down)
    $rawX = [int][Math]::Round($X * $Scale)
    $rawY = [int][Math]::Round($Y * $Scale)
    $buttons = if ($Down) { [IntPtr]::new(1) } else { [IntPtr]::Zero }
    [NativeMosaicBrushWindow]::SendMessage(
        $Window, $Message, $buttons, [NativeMosaicBrushWindow]::Point($rawX, $rawY)) | Out-Null
}

function Assert-UniformCell {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [int]$Left,
        [int]$Top,
        [System.Drawing.Color]$Expected,
        [string]$Label
    )
    $colors = [Collections.Generic.HashSet[int]]::new()
    for ($y = $Top; $y -lt $Top + 9; $y++) {
        for ($x = $Left; $x -lt $Left + 9; $x++) {
            $pixel = $Bitmap.GetPixel($x, $y)
            $colors.Add(($pixel.R -shl 16) -bor ($pixel.G -shl 8) -bor $pixel.B) | Out-Null
        }
    }
    if ($colors.Count -gt 1) {
        throw "FAIL: $Label contains $($colors.Count) colors instead of one stable mosaic block."
    }
    $actual = $Bitmap.GetPixel($Left + 4, $Top + 4)
    $delta = [Math]::Abs($actual.R - $Expected.R) +
        [Math]::Abs($actual.G - $Expected.G) +
        [Math]::Abs($actual.B - $Expected.B)
    if ($delta -gt 3) {
        throw "FAIL: $Label uses $actual instead of the source-grid average $Expected."
    }
}

$bitmap = [System.Drawing.Bitmap]::new(800, 600, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
    for ($y = 0; $y -lt $bitmap.Height; $y++) {
        for ($x = 0; $x -lt $bitmap.Width; $x++) {
            $bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(
                255, ($x + $y) % 256, ($y * 3) % 256, ($x * 2) % 256))
        }
    }
    $bitmap.Save($fixture, [System.Drawing.Imaging.ImageFormat]::Bmp)
} finally {
    $bitmap.Dispose()
}

try {
    $process = Start-Process -FilePath $EditorPath -ArgumentList @(
        '--select-file', $fixture, '--output', $output, '--lang', '2', '--allow-window-capture'
    ) -PassThru
    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 60 -and $window -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 40
        $window = [NativeMosaicBrushWindow]::Find([uint32]$process.Id)
    }
    if ($window -eq [IntPtr]::Zero) { throw 'FAIL: native region window was not shown.' }
    $scale = 96.0 / [NativeMosaicBrushWindow]::GetDpiForWindow($window)

    Send-Mouse $window 0x0201 40 60 $scale $true
    Send-Mouse $window 0x0200 700 450 $scale $true
    Send-Mouse $window 0x0202 700 450 $scale $false
    [NativeMosaicBrushWindow]::UpdateWindow($window) | Out-Null

    $toolbarLeft = 12
    $toolbarTop = 460
    Send-Mouse $window 0x0201 ($toolbarLeft + 230 + 23) ($toolbarTop + 31) $scale $true
    Send-Mouse $window 0x0202 ($toolbarLeft + 230 + 23) ($toolbarTop + 31) $scale $false

    Send-Mouse $window 0x0201 220 250 $scale $true
    foreach ($x in 240,260,280,300,320,340,360,380,400,420,440,460,480,500,520) {
        Send-Mouse $window 0x0200 $x 250 $scale $true
    }
    [NativeMosaicBrushWindow]::UpdateWindow($window) | Out-Null

    $preview = [System.Drawing.Bitmap]::new(800, 600, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($preview)
        $destination = $graphics.GetHdc()
        try {
            [NativeMosaicBrushWindow]::PrintWindow($window, $destination, 3) | Out-Null
        } finally {
            $graphics.ReleaseHdc($destination)
            $graphics.Dispose()
        }
        Assert-UniformCell $preview 315 243 ([System.Drawing.Color]::FromArgb(255, 54, 229, 126)) 'live mosaic preview cell'
        if (-not [string]::IsNullOrWhiteSpace($PreviewPath)) {
            $preview.Save($PreviewPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
    } finally {
        $preview.Dispose()
    }

    Send-Mouse $window 0x0202 520 250 $scale $false
    [NativeMosaicBrushWindow]::UpdateWindow($window) | Out-Null
    Send-Mouse $window 0x0201 ($toolbarLeft + 755 + 23) ($toolbarTop + 31) $scale $true
    Send-Mouse $window 0x0202 ($toolbarLeft + 755 + 23) ($toolbarTop + 31) $scale $false
    if (-not $process.WaitForExit(5000)) { throw 'FAIL: mosaic Copy did not close the editor.' }

    $clipboard = $null
    for ($attempt = 0; $attempt -lt 30 -and -not $clipboard; $attempt++) {
        try { $clipboard = [System.Windows.Forms.Clipboard]::GetImage() } catch { }
        if (-not $clipboard) { Start-Sleep -Milliseconds 100 }
    }
    if (-not $clipboard) { throw 'FAIL: mosaic Copy produced no bitmap.' }
    try {
        Assert-UniformCell ([System.Drawing.Bitmap]$clipboard) 275 183 ([System.Drawing.Color]::FromArgb(255, 54, 229, 126)) 'committed mosaic output cell'
    } finally {
        $clipboard.Dispose()
    }
    Write-Output 'PASS: freehand mosaic shows and copies stable source-grid pixel blocks.'
} finally {
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    Remove-Item -LiteralPath $fixture,$output -Force -ErrorAction SilentlyContinue
}
