param(
    [Parameter(Mandatory = $true)]
    [string]$EditorPath
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativePreviewInteractionApi {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr value);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct ScreenPoint { public int X, Y; }
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindowsProc callback, IntPtr value);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd, System.Text.StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hwnd, ref ScreenPoint point);
    [DllImport("user32.dll")] public static extern bool IsZoomed(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr destination, uint flags);
    public static IntPtr Find(uint targetProcessId) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((hwnd, _) => {
            uint processId;
            GetWindowThreadProcessId(hwnd, out processId);
            var className = new System.Text.StringBuilder(128);
            GetClassName(hwnd, className, className.Capacity);
            if (processId == targetProcessId && IsWindowVisible(hwnd) &&
                className.ToString() == "HdrSdrNativeEditorPreviewWindow") {
                result = hwnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
    public static IntPtr LParamPoint(int x, int y) { return new IntPtr((y << 16) | (x & 0xffff)); }
    public static IntPtr Wheel(int delta) { return new IntPtr((delta & 0xffff) << 16); }
    public static int[] FindRedBounds(byte[] pixels, int stride, int width, int height) {
        int minX = width, maxX = -1, minY = height, maxY = -1;
        for (int y = 44; y < height - 82; y += 2) {
            for (int x = 0; x < width; x += 2) {
                int offset = y * stride + x * 4;
                byte blue = pixels[offset];
                byte green = pixels[offset + 1];
                byte red = pixels[offset + 2];
                if (red > 170 && green < 80 && blue < 100) {
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }
        return new[] { minX, minY, maxX, maxY };
    }
}
'@

$previousDpiContext = [NativePreviewInteractionApi]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))

function Get-ClientSize([IntPtr]$Window) {
    $rect = New-Object NativePreviewInteractionApi+Rect
    if (-not [NativePreviewInteractionApi]::GetClientRect($Window, [ref]$rect)) {
        throw 'FAIL: could not read native preview client bounds.'
    }
    return @(
        ($rect.Right - $rect.Left),
        ($rect.Bottom - $rect.Top)
    )
}

function Invoke-TitleButton([IntPtr]$Window) {
    $size = Get-ClientSize $Window
    $x = [int]$size[0] - 72
    $y = 21
    $point = [NativePreviewInteractionApi]::LParamPoint($x, $y)
    $script:ButtonDiagnostics = "client=$($size[0])x$($size[1]), dpi=$([NativePreviewInteractionApi]::GetDpiForWindow($Window)), posted=$x,$y"
    [NativePreviewInteractionApi]::PostMessage($Window, 0x0201, [IntPtr]::new(1), $point) | Out-Null
    [NativePreviewInteractionApi]::PostMessage($Window, 0x0202, [IntPtr]::Zero, $point) | Out-Null
}

function Wait-ZoomState([IntPtr]$Window, [bool]$Expected) {
    for ($attempt = 0; $attempt -lt 40; $attempt++) {
        if ([NativePreviewInteractionApi]::IsZoomed($Window) -eq $Expected) { return }
        Start-Sleep -Milliseconds 50
    }
    throw "FAIL: custom title button did not switch IsZoomed to $Expected ($script:ButtonDiagnostics)."
}

function Get-RedBounds([IntPtr]$Window) {
    $size = Get-ClientSize $Window
    $width = [int]$size[0]
    $height = [int]$size[1]
    $bitmap = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        try {
            $dc = $graphics.GetHdc()
            try {
                if (-not [NativePreviewInteractionApi]::PrintWindow($Window, $dc, 3)) {
                    throw 'FAIL: PrintWindow could not capture the native preview.'
                }
            } finally {
                $graphics.ReleaseHdc($dc)
            }
        } finally {
            $graphics.Dispose()
        }

        $lockRect = [Drawing.Rectangle]::new(0, 0, $width, $height)
        $data = $bitmap.LockBits($lockRect, [Drawing.Imaging.ImageLockMode]::ReadOnly,
                                 [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $stride = [Math]::Abs($data.Stride)
            $bytes = [byte[]]::new($stride * $height)
            [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
            $bounds = [NativePreviewInteractionApi]::FindRedBounds(
                $bytes, $stride, $width, $height)
            if ($bounds[2] -lt $bounds[0] -or $bounds[3] -lt $bounds[1]) {
                throw 'FAIL: test image was not visible in the native preview.'
            }
            return $bounds
        } finally {
            $bitmap.UnlockBits($data)
        }
    } finally {
        $bitmap.Dispose()
    }
}

$fixture = Join-Path $env:TEMP "hdr-sdr-native-preview-interaction-$PID.bmp"
$output = "$fixture.png"
$bitmap = [Drawing.Bitmap]::new(1200, 900)
try {
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try { $graphics.Clear([Drawing.Color]::FromArgb(220, 35, 45)) }
    finally { $graphics.Dispose() }
    $bitmap.Save($fixture, [Drawing.Imaging.ImageFormat]::Bmp)
} finally {
    $bitmap.Dispose()
}

$process = $null
try {
    $process = Start-Process -FilePath $EditorPath -ArgumentList @(
        '--edit-file', $fixture, '--output', $output, '--lang', '2', '--skip-initial-copy'
    ) -PassThru
    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 50 -and $window -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 50
        $window = [NativePreviewInteractionApi]::Find([uint32]$process.Id)
    }
    if ($window -eq [IntPtr]::Zero) { throw 'FAIL: native preview did not open.' }

    Invoke-TitleButton $window
    Wait-ZoomState $window $true
    Invoke-TitleButton $window
    Wait-ZoomState $window $false

    $before = Get-RedBounds $window
    $size = Get-ClientSize $window
    $anchor = New-Object NativePreviewInteractionApi+ScreenPoint
    $anchor.X = [int]$size[0] / 2
    $anchor.Y = [int]$size[1] / 2
    [NativePreviewInteractionApi]::ClientToScreen($window, [ref]$anchor) | Out-Null
    [NativePreviewInteractionApi]::PostMessage(
        $window, 0x020A, [NativePreviewInteractionApi]::Wheel(120),
        [NativePreviewInteractionApi]::LParamPoint($anchor.X, $anchor.Y)) | Out-Null
    Start-Sleep -Milliseconds 150
    $after = Get-RedBounds $window
    $beforeWidth = [int]$before[2] - [int]$before[0]
    $afterWidth = [int]$after[2] - [int]$after[0]
    if ($afterWidth -lt [int]($beforeWidth * 1.12)) {
        throw "FAIL: mouse wheel did not visibly enlarge the preview ($beforeWidth -> $afterWidth)."
    }

    [NativePreviewInteractionApi]::PostMessage(
        $window, 0x0100, [IntPtr]::new(0x1B), [IntPtr]::Zero) | Out-Null
    if (-not $process.WaitForExit(3000)) { throw 'FAIL: Esc did not close the preview.' }
    Write-Output 'PASS: native preview maximizes, restores, and visibly zooms around the mouse wheel.'
} finally {
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    Remove-Item -LiteralPath $fixture,$output -Force -ErrorAction SilentlyContinue
    if ($previousDpiContext -ne [IntPtr]::Zero) {
        [NativePreviewInteractionApi]::SetThreadDpiAwarenessContext($previousDpiContext) | Out-Null
    }
}
