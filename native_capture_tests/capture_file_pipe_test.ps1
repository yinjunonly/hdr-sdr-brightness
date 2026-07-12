param(
    [string]$NativeCapturePath
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$nativeCapture = if ($NativeCapturePath) { $NativeCapturePath } else { Join-Path $root 'bin\capture\HdrSdrNativeCapture.exe' }
$pipeName = "HdrSdrNativeCaptureTest-$PID-$([Guid]::NewGuid().ToString('N'))"
$output = Join-Path $env:TEMP "$pipeName.bmp"
$process = $null
$client = $null
try {
    $process = Start-Process -FilePath $nativeCapture -ArgumentList @(
        '--server-once', '--pipe-name', $pipeName, '--parent-pid', $PID
    ) -WindowStyle Hidden -PassThru

    $client = [System.IO.Pipes.NamedPipeClientStream]::new(
        '.', $pipeName, [System.IO.Pipes.PipeDirection]::InOut)
    $client.Connect(5000)
    $writer = [System.IO.StreamWriter]::new($client, [System.Text.Encoding]::Unicode, 1024, $true)
    $reader = [System.IO.StreamReader]::new($client, [System.Text.Encoding]::Unicode, $false, 1024, $true)
    try {
        $writer.AutoFlush = $true
        $writer.WriteLine("capture-file`t2`t$output`t3500")
        $response = $reader.ReadLine()
    } finally {
        $writer.Dispose()
        $reader.Dispose()
    }

    if ($response -ne '0') { throw "FAIL: capture-file returned '$response'." }
    if (-not (Test-Path -LiteralPath $output)) { throw 'FAIL: capture-file produced no BMP.' }
    Add-Type -AssemblyName System.Drawing
    $image = [System.Drawing.Image]::FromFile($output)
    try {
        if ($image.Width -lt 1 -or $image.Height -lt 1) { throw 'FAIL: capture-file produced an empty BMP.' }
        Write-Output "PASS: native pipe saved a $($image.Width)x$($image.Height) BMP without clipboard output."
    } finally {
        $image.Dispose()
    }
} finally {
    if ($client) { $client.Dispose() }
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
