param(
    [Parameter(Mandatory = $true)]
    [string]$EditorPath
)

$ErrorActionPreference = 'Stop'
$fixture = Join-Path $env:TEMP "hdr-sdr-native-preview-$PID.bmp"
$output = [IO.Path]::ChangeExtension($fixture, '.png')
$process = $null

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativePreviewEditWindow {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr value);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr value);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr hwnd, uint attribute, out Rect rect, int size);
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
    [NativePreviewEditWindow]::PostMessage(
        $Window, $Message, $buttons, [NativePreviewEditWindow]::Point($rawX, $rawY)) | Out-Null
}

$bitmap = [System.Drawing.Bitmap]::new(640, 360, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try { $graphics.Clear([System.Drawing.Color]::FromArgb(255, 24, 40, 56)) } finally { $graphics.Dispose() }
    $bitmap.Save($fixture, [System.Drawing.Imaging.ImageFormat]::Bmp)
} finally {
    $bitmap.Dispose()
}

try {
    $process = Start-Process -FilePath $EditorPath -ArgumentList @(
        '--edit-file', $fixture, '--output', $output, '--lang', '2', '--skip-initial-copy'
    ) -PassThru
    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 50 -and $window -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 50
        $window = [NativePreviewEditWindow]::Find([uint32]$process.Id)
    }
    if ($window -eq [IntPtr]::Zero) { throw 'FAIL: native preview editor was not shown.' }
    $bounds = New-Object NativePreviewEditWindow+Rect
    [NativePreviewEditWindow]::DwmGetWindowAttribute(
        $window, 9, [ref]$bounds, [Runtime.InteropServices.Marshal]::SizeOf($bounds)) | Out-Null
    $clientWidth = $bounds.Right - $bounds.Left
    $clientHeight = $bounds.Bottom - $bounds.Top
    $messageScale = 96.0 / [NativePreviewEditWindow]::GetDpiForWindow($window)

    $toolbarLeft = [int](($clientWidth - 820) / 2)
    $toolbarTop = $clientHeight - 72
    $markerX = $toolbarLeft + 68 + 23
    $colorX = $toolbarLeft + 284 + 23
    $undoX = $toolbarLeft + 351 + 23
    $redoX = $toolbarLeft + 405 + 23
    $copyX = $toolbarLeft + 755 + 23
    $buttonY = $toolbarTop + 31

    Send-Mouse $window 0x0201 $markerX $buttonY $messageScale $true
    Send-Mouse $window 0x0202 $markerX $buttonY $messageScale $false
    Send-Mouse $window 0x0201 $markerX $buttonY $messageScale $true
    Send-Mouse $window 0x0202 $markerX $buttonY $messageScale $false
    $optionY = $toolbarTop - 32
    Send-Mouse $window 0x0201 ($markerX + 50) $optionY $messageScale $true
    Send-Mouse $window 0x0202 ($markerX + 50) $optionY $messageScale $false

    Send-Mouse $window 0x0201 $colorX $buttonY $messageScale $true
    Send-Mouse $window 0x0202 $colorX $buttonY $messageScale $false
    Send-Mouse $window 0x0201 ($colorX + 25) $optionY $messageScale $true
    Send-Mouse $window 0x0202 ($colorX + 25) $optionY $messageScale $false

    $availableWidth = $clientWidth - 40
    $availableHeight = $clientHeight - 82 - 52
    $ratio = [Math]::Min($availableWidth / 640.0, $availableHeight / 360.0)
    $imageWidth = [int][Math]::Round(640 * $ratio)
    $imageHeight = [int][Math]::Round(360 * $ratio)
    $imageLeft = [int](20 + ($availableWidth - $imageWidth) / 2)
    $imageTop = [int](52 + ($availableHeight - $imageHeight) / 2)
    $startX = [int][Math]::Round($imageLeft + 100 * $ratio)
    $startY = [int][Math]::Round($imageTop + 80 * $ratio)
    $endX = [int][Math]::Round($imageLeft + 300 * $ratio)
    $endY = [int][Math]::Round($imageTop + 220 * $ratio)
    Send-Mouse $window 0x0201 $startX $startY $messageScale $true
    Send-Mouse $window 0x0200 $endX $endY $messageScale $true
    Send-Mouse $window 0x0202 $endX $endY $messageScale $false
    Start-Sleep -Milliseconds 100

    Send-Mouse $window 0x0201 $undoX $buttonY $messageScale $true
    Send-Mouse $window 0x0202 $undoX $buttonY $messageScale $false
    Send-Mouse $window 0x0201 $redoX $buttonY $messageScale $true
    Send-Mouse $window 0x0202 $redoX $buttonY $messageScale $false
    Send-Mouse $window 0x0201 $copyX $buttonY $messageScale $true
    Send-Mouse $window 0x0202 $copyX $buttonY $messageScale $false

    if (-not $process.WaitForExit(5000)) { throw 'FAIL: preview Copy did not close the editor.' }
    $dataObject = [System.Windows.Forms.Clipboard]::GetDataObject()
    $pngStream = $dataObject.GetData('PNG')
    if ($pngStream -isnot [System.IO.Stream]) {
        throw 'FAIL: preview Copy did not publish the registered PNG clipboard format.'
    }
    try {
        $pngStream.Position = 0
        $pngBytes = [byte[]]::new([int]$pngStream.Length)
        $read = $pngStream.Read($pngBytes, 0, $pngBytes.Length)
        $signature = [byte[]](0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a)
        $iend = [byte[]](0, 0, 0, 0, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82)
        $signatureHex = [BitConverter]::ToString($signature)
        $iendHex = [BitConverter]::ToString($iend)
        if ($read -ne $pngBytes.Length -or $pngBytes.Length -lt 24 -or
            [BitConverter]::ToString($pngBytes, 0, 8) -ne $signatureHex -or
            [BitConverter]::ToString($pngBytes, $pngBytes.Length - 12, 12) -ne $iendHex) {
            throw 'FAIL: registered PNG clipboard bytes are truncated or contain trailing allocation bytes.'
        }
    } finally {
        $pngStream.Dispose()
    }
    $clipboard = $null
    for ($attempt = 0; $attempt -lt 30 -and -not $clipboard; $attempt++) {
        try { $clipboard = [System.Windows.Forms.Clipboard]::GetImage() } catch { }
        if (-not $clipboard) { Start-Sleep -Milliseconds 100 }
    }
    if (-not $clipboard) { throw 'FAIL: preview Copy produced no bitmap.' }
    try {
        if ($clipboard.Width -ne 640 -or $clipboard.Height -ne 360) {
            throw "FAIL: edited preview is $($clipboard.Width)x$($clipboard.Height)."
        }
        $pixel = ([System.Drawing.Bitmap]$clipboard).GetPixel(100, 150)
        if ($pixel.R -gt 130 -or $pixel.G -lt 110 -or $pixel.B -lt 180) {
            throw "FAIL: selected 6 px blue annotation was not restored by Redo (pixel $pixel)."
        }
    } finally {
        $clipboard.Dispose()
    }
    Write-Output 'PASS: native preview size/color options, marker, Undo, Redo, and Copy are wired to physical pixels.'
} finally {
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    Remove-Item -LiteralPath $fixture,$output -Force -ErrorAction SilentlyContinue
}
