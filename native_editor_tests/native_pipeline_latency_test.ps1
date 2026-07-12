param(
    [Parameter(Mandatory = $true)]
    [string]$EditorPath,
    [Parameter(Mandatory = $true)]
    [string]$NativeCapturePath,
    [int]$MaximumVisibleMs = 250
)

$ErrorActionPreference = 'Stop'
$suffix = "$PID-$([Guid]::NewGuid().ToString('N'))"
$pipeName = "HdrSdrNativePipeline-$suffix"
$fixture = Join-Path $env:TEMP "$suffix.bmp"
$nativeProcess = $null
$editorProcess = $null
$client = $null

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativePipelineWindow {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr value);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr value);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
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
}
'@

try {
    $warmup = Start-Process -FilePath $EditorPath -ArgumentList @('--warmup') -PassThru
    if (-not $warmup.WaitForExit(5000) -or $warmup.ExitCode -ne 0) {
        throw 'FAIL: native editor warmup did not finish cleanly.'
    }
    $nativeProcess = Start-Process -FilePath $NativeCapturePath -ArgumentList @(
        '--server', '--pipe-name', $pipeName, '--parent-pid', $PID
    ) -WindowStyle Hidden -PassThru
    $client = [System.IO.Pipes.NamedPipeClientStream]::new(
        '.', $pipeName, [System.IO.Pipes.PipeDirection]::InOut)
    $client.Connect(5000)
    $writer = [System.IO.StreamWriter]::new($client, [System.Text.Encoding]::Unicode, 1024, $true)
    $reader = [System.IO.StreamReader]::new($client, [System.Text.Encoding]::Unicode, $false, 1024, $true)
    try {
        $writer.AutoFlush = $true
        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        $writer.WriteLine("capture-file`t2`t$fixture`t3500")
        $response = $reader.ReadLine()
        if ($response -ne '0') { throw "Native capture returned '$response'." }
        $captureMs = $stopwatch.Elapsed.TotalMilliseconds

        $editorProcess = Start-Process -FilePath $EditorPath -ArgumentList @(
            '--select-file', $fixture, '--output', "$fixture.png", '--lang', '2'
        ) -PassThru
        $window = [IntPtr]::Zero
        while ($stopwatch.ElapsedMilliseconds -lt 3000 -and $window -eq [IntPtr]::Zero) {
            $window = [NativePipelineWindow]::Find([uint32]$editorProcess.Id)
            if ($window -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 2 }
        }
        $visibleMs = $stopwatch.Elapsed.TotalMilliseconds
    } finally {
        $writer.Dispose()
        $reader.Dispose()
    }

    if ($window -eq [IntPtr]::Zero) { throw 'FAIL: native region editor did not become visible.' }
    if ($visibleMs -gt $MaximumVisibleMs) {
        $editorMs = $visibleMs - $captureMs
        throw "FAIL: native capture-to-visible took $([Math]::Round($visibleMs, 1)) ms (capture $([Math]::Round($captureMs, 1)) ms, editor $([Math]::Round($editorMs, 1)) ms); limit is $MaximumVisibleMs ms."
    }
    Write-Output "PASS: native capture $([Math]::Round($captureMs, 1)) ms; cold C++ editor visible $([Math]::Round($visibleMs, 1)) ms total."
} finally {
    if ($editorProcess -and -not $editorProcess.HasExited) {
        $window = [NativePipelineWindow]::Find([uint32]$editorProcess.Id)
        if ($window -ne [IntPtr]::Zero) {
            [NativePipelineWindow]::PostMessage($window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        }
        if (-not $editorProcess.WaitForExit(2000)) { Stop-Process -Id $editorProcess.Id -Force }
    }
    if ($client) { $client.Dispose() }
    if ($nativeProcess -and -not $nativeProcess.HasExited) { Stop-Process -Id $nativeProcess.Id -Force }
    Remove-Item -LiteralPath $fixture,"$fixture.png" -Force -ErrorAction SilentlyContinue
}
