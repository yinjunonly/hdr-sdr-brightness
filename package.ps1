param(
    [string]$Version
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$dist = Join-Path $root 'dist'
$bin = Join-Path $root 'bin'
$exe = Join-Path $bin 'HdrSdrBrightness.exe'

if ([string]::IsNullOrWhiteSpace($Version)) {
    $versionFile = Join-Path $root 'VERSION'
    if (-not (Test-Path -LiteralPath $versionFile)) {
        throw "Missing version file: $versionFile"
    }
    $Version = (Get-Content -LiteralPath $versionFile -Raw).Trim()
}

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    throw "Version must use MAJOR.MINOR.PATCH, for example 1.0.0"
}

$zip = Join-Path $dist "HdrSdrBrightness-$Version-win64.zip"

& (Join-Path $root 'build.ps1') -Clean -Version $Version

if (-not (Test-Path -LiteralPath $exe)) {
    throw "Missing executable: $exe"
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null
if (Test-Path -LiteralPath $zip) {
    Remove-Item -LiteralPath $zip -Force
}

$packageRoot = Join-Path $dist "HdrSdrBrightness-$Version-win64"
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
Copy-Item -LiteralPath $exe -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination $packageRoot
$imageDir = Join-Path $root 'image'
if (Test-Path -LiteralPath $imageDir) {
    Copy-Item -LiteralPath $imageDir -Destination $packageRoot -Recurse
}

Compress-Archive -Path (Join-Path $packageRoot '*') -DestinationPath $zip -CompressionLevel Optimal
Remove-Item -LiteralPath $packageRoot -Recurse -Force

Write-Host "Packaged $zip"
