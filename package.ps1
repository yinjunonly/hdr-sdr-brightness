param(
    [string]$Version
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$dist = Join-Path $root 'dist'
$obj = Join-Path $root 'obj'
$buildRoot = Join-Path $obj 'package\desktop-build'
$exe = Join-Path $buildRoot 'HdrSdrBrightness.exe'
$captureDir = Join-Path $buildRoot 'capture'

function Assert-PathInside {
    param(
        [string]$Root,
        [string]$Path
    )

    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $pathFull = [IO.Path]::GetFullPath($Path)
    if (-not $pathFull.StartsWith($rootFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to operate outside project root: $pathFull"
    }
}

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

Assert-PathInside -Root $root -Path $buildRoot
if (Test-Path -LiteralPath $buildRoot) {
    Remove-Item -LiteralPath $buildRoot -Recurse -Force
}

& (Join-Path $root 'build.ps1') -Version $Version -OutputDir $buildRoot
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not (Test-Path -LiteralPath $exe)) {
    throw "Missing executable: $exe"
}
if (-not (Test-Path -LiteralPath (Join-Path $captureDir 'HdrSdrNativeCapture.exe'))) {
    throw "Missing native capture helper: $captureDir"
}
if (-not (Test-Path -LiteralPath (Join-Path $captureDir 'HdrSdrNativeEditor.exe'))) {
    throw "Missing editor helper: $captureDir"
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null
Assert-PathInside -Root $root -Path $zip
if (Test-Path -LiteralPath $zip) {
    Remove-Item -LiteralPath $zip -Force
}

$packageRoot = Join-Path $dist "HdrSdrBrightness-$Version-win64"
Assert-PathInside -Root $root -Path $packageRoot
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
Copy-Item -LiteralPath $exe -Destination $packageRoot
Copy-Item -LiteralPath $captureDir -Destination (Join-Path $packageRoot 'capture') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination $packageRoot

Compress-Archive -Path (Join-Path $packageRoot '*') -DestinationPath $zip -CompressionLevel Optimal
Remove-Item -LiteralPath $packageRoot -Recurse -Force

Write-Host "Packaged $zip"
