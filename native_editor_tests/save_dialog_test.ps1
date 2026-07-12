param(
    [Parameter(Mandatory = $true)]
    [string]$EditorPath
)

$ErrorActionPreference = 'Stop'
$fixture = Join-Path $env:TEMP "hdr-sdr-native-save-$PID.bmp"
$output = Join-Path $env:TEMP "hdr-sdr-native-save-$PID.png"
$process = $null

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativeSaveDialogWindow {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr value);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr value);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, System.Text.StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr hwnd, uint attribute, out Rect rect, int size);
    private static string ClassName(IntPtr hwnd) {
        var text = new System.Text.StringBuilder(128);
        GetClassName(hwnd, text, text.Capacity);
        return text.ToString();
    }
    public static IntPtr Find(uint processId, bool dialog) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((hwnd, _) => {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner == processId && IsWindowVisible(hwnd) && (ClassName(hwnd) == "#32770") == dialog) {
                result = hwnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
    public static IntPtr Point(int x, int y) { return new IntPtr((y << 16) | (x & 0xffff)); }
}
'@

$bitmap = [System.Drawing.Bitmap]::new(640, 360, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try { $graphics.Clear([System.Drawing.Color]::FromArgb(255, 32, 48, 64)) } finally { $graphics.Dispose() }
    $bitmap.Save($fixture, [System.Drawing.Imaging.ImageFormat]::Bmp)
} finally {
    $bitmap.Dispose()
}
Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue

try {
    $process = Start-Process -FilePath $EditorPath -ArgumentList @(
        '--edit-file', $fixture, '--output', $output, '--lang', '2', '--skip-initial-copy'
    ) -PassThru
    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 50 -and $window -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 50
        $window = [NativeSaveDialogWindow]::Find([uint32]$process.Id, $false)
    }
    if ($window -eq [IntPtr]::Zero) { throw 'FAIL: native preview window was not shown for Save As.' }

    $bounds = New-Object NativeSaveDialogWindow+Rect
    [NativeSaveDialogWindow]::DwmGetWindowAttribute(
        $window, 9, [ref]$bounds, [Runtime.InteropServices.Marshal]::SizeOf($bounds)) | Out-Null
    $clientWidth = $bounds.Right - $bounds.Left
    $clientHeight = $bounds.Bottom - $bounds.Top
    $messageScale = 96.0 / [NativeSaveDialogWindow]::GetDpiForWindow($window)
    $saveX = [int](($clientWidth - 820) / 2) + 701 + 23
    $saveY = $clientHeight - 72 + 31
    $rawX = [int][Math]::Round($saveX * $messageScale)
    $rawY = [int][Math]::Round($saveY * $messageScale)
    $point = [NativeSaveDialogWindow]::Point($rawX, $rawY)
    [NativeSaveDialogWindow]::PostMessage($window, 0x0201, [IntPtr]::new(1), $point) | Out-Null
    [NativeSaveDialogWindow]::PostMessage($window, 0x0202, [IntPtr]::Zero, $point) | Out-Null

    $dialog = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 50 -and $dialog -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 50
        $dialog = [NativeSaveDialogWindow]::Find([uint32]$process.Id, $true)
    }
    if ($dialog -eq [IntPtr]::Zero) { throw 'FAIL: native Save action did not open the Save As dialog.' }
    [NativeSaveDialogWindow]::PostMessage($dialog, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Write-Output 'PASS: native Save action opens the Windows Save As dialog.'
} finally {
    if ($process -and -not $process.HasExited) {
        $window = [NativeSaveDialogWindow]::Find([uint32]$process.Id, $false)
        if ($window -ne [IntPtr]::Zero) {
            [NativeSaveDialogWindow]::PostMessage($window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        }
        if (-not $process.WaitForExit(2000)) { Stop-Process -Id $process.Id -Force }
    }
    Remove-Item -LiteralPath $fixture,$output -Force -ErrorAction SilentlyContinue
}
