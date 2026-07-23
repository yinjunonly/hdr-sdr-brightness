param(
    [Parameter(Mandatory = $true)]
    [string]$BuildRoot
)

$ErrorActionPreference = 'Stop'

function Find-WindowsSdkTool {
    param([string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $kitBin = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
    if (Test-Path -LiteralPath $kitBin) {
        $matches = Get-ChildItem -LiteralPath $kitBin -Recurse -Filter $Name -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\x64\\' } |
            Sort-Object FullName -Descending
        if ($matches) {
            return $matches[0].FullName
        }
    }

    throw "Missing Windows SDK tool: $Name"
}

$resolvedBuildRoot = [IO.Path]::GetFullPath($BuildRoot)
$exe = Join-Path $resolvedBuildRoot 'HdrSdrBrightness.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Missing executable: $exe"
}

$mt = Find-WindowsSdkTool -Name 'mt.exe'
$manifestPath = Join-Path ([IO.Path]::GetTempPath()) ("hdr-sdr-brightness-{0}.manifest" -f [Guid]::NewGuid())

try {
    & $mt -nologo "-inputresource:$exe;#1" "-out:$manifestPath"
    if ($LASTEXITCODE -ne 0) {
        throw "mt.exe failed to extract the final executable manifest."
    }

    $manifest = Get-Content -LiteralPath $manifestPath -Raw
    if ($manifest -notmatch '<dpiAwareness[^>]*>\s*PerMonitorV2,\s*PerMonitor\s*</dpiAwareness>') {
        throw 'FAIL: final executable is not manifested as Per-Monitor V2 DPI aware.'
    }
    if ($manifest -notmatch 'Microsoft\.Windows\.Common-Controls' -or
        $manifest -notmatch 'version="6\.0\.0\.0"') {
        throw 'FAIL: final executable is missing the Common Controls v6 dependency.'
    }
    if ($manifest -notmatch '8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a') {
        throw 'FAIL: final executable is missing the Windows 10 compatibility declaration.'
    }
} finally {
    if (Test-Path -LiteralPath $manifestPath) {
        Remove-Item -LiteralPath $manifestPath -Force
    }
}

Write-Output 'PASS: final executable embeds the complete Per-Monitor V2 manifest.'
