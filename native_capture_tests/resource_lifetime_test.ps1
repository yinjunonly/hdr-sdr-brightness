param(
    [string]$NativeCapturePath,
    [int]$CaptureCount = 20,
    [int]$MaximumThreadGrowth = 2,
    [int]$MaximumHandleGrowth = 20
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$nativeCapture = if ($NativeCapturePath) {
    $NativeCapturePath
} else {
    Join-Path $root 'bin\capture\HdrSdrNativeCapture.exe'
}
$pipeName = "HdrSdrNativeLifetime-$PID-$([Guid]::NewGuid().ToString('N'))"
$output = Join-Path $env:TEMP "$pipeName.bmp"
$process = $null

function Invoke-NativeCapture {
    $client = [System.IO.Pipes.NamedPipeClientStream]::new(
        '.', $pipeName, [System.IO.Pipes.PipeDirection]::InOut)
    try {
        $client.Connect(5000)
        $writer = [System.IO.StreamWriter]::new(
            $client, [System.Text.Encoding]::Unicode, 1024, $true)
        $reader = [System.IO.StreamReader]::new(
            $client, [System.Text.Encoding]::Unicode, $false, 1024, $true)
        try {
            $writer.AutoFlush = $true
            $writer.WriteLine("capture-file`t2`t$output`t3500")
            $response = $reader.ReadLine()
        } finally {
            $writer.Dispose()
            $reader.Dispose()
        }
    } finally {
        $client.Dispose()
    }

    if ($response -ne '0') {
        throw "FAIL: capture-file returned '$response'."
    }
}

function Get-ResourceSample {
    $process.Refresh()
    return [pscustomobject]@{
        Handles = $process.HandleCount
        Threads = $process.Threads.Count
        WorkingMiB = [Math]::Round($process.WorkingSet64 / 1MB, 2)
        PrivateMiB = [Math]::Round($process.PrivateMemorySize64 / 1MB, 2)
    }
}

try {
    $process = Start-Process -FilePath $nativeCapture -ArgumentList @(
        '--server', '--pipe-name', $pipeName, '--parent-pid', $PID
    ) -WindowStyle Hidden -PassThru

    Invoke-NativeCapture
    Start-Sleep -Milliseconds 150
    $baseline = Get-ResourceSample

    for ($index = 0; $index -lt $CaptureCount; $index++) {
        Invoke-NativeCapture
    }
    Start-Sleep -Milliseconds 250
    $final = Get-ResourceSample

    $threadGrowth = $final.Threads - $baseline.Threads
    $handleGrowth = $final.Handles - $baseline.Handles
    Write-Output ("Native lifetime: threads {0}->{1} ({2:+#;-#;0}), handles {3}->{4} ({5:+#;-#;0}), private {6}->{7} MiB." -f `
        $baseline.Threads, $final.Threads, $threadGrowth,
        $baseline.Handles, $final.Handles, $handleGrowth,
        $baseline.PrivateMiB, $final.PrivateMiB)

    if ($threadGrowth -gt $MaximumThreadGrowth) {
        throw "FAIL: $CaptureCount repeated captures leaked $threadGrowth threads; limit is $MaximumThreadGrowth."
    }
    if ($handleGrowth -gt $MaximumHandleGrowth) {
        throw "FAIL: $CaptureCount repeated captures leaked $handleGrowth handles; limit is $MaximumHandleGrowth."
    }

    Write-Output "PASS: repeated native captures keep process resources bounded."
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
    }
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
