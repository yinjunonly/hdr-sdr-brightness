param(
    [Parameter(Mandatory = $true)]
    [string]$Version,
    [switch]$Clean,
    [switch]$AllowDirtySource,
    [switch]$SkipBuild,
    [switch]$SkipUpload,
    [string]$AppxSymPath,
    [string]$MakeAppxPath
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$obj = Join-Path $root 'obj'
$bin = Join-Path $root 'bin'
$dist = Join-Path $root 'dist'
$store = Join-Path $root 'store'
$buildRoot = Join-Path $obj 'msix\build'
$payloadRoot = if ($SkipBuild) { $bin } else { $buildRoot }
$exe = Join-Path $payloadRoot 'HdrSdrBrightness.exe'
$captureDir = Join-Path $payloadRoot 'capture'
$manifestTemplate = Join-Path $store 'AppxManifest.xml.in'
$packageRoot = Join-Path $obj 'msix\package'
$assetsRoot = Join-Path $packageRoot 'Assets'
$uploadRoot = Join-Path $obj 'msix\upload'

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    throw "Version must use MAJOR.MINOR.PATCH, for example 1.0.6"
}

& (Join-Path $root 'release-preflight.ps1') -AllowDirtySource:$AllowDirtySource
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

function Convert-ToMsixVersion {
    param([string]$Value)
    $parts = @($Value.Split('.')) + @('0')
    foreach ($part in $parts) {
        $number = [int]$part
        if ($number -lt 0 -or $number -gt 65535) {
            throw "Version part out of range for MSIX: $part"
        }
    }
    return $parts -join '.'
}

function Find-WindowsSdkTool {
    param(
        [string]$Name,
        [string]$RequestedPath
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
        if (-not (Test-Path -LiteralPath $RequestedPath)) {
            throw "Missing tool: $RequestedPath"
        }
        return (Resolve-Path -LiteralPath $RequestedPath).Path
    }

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

    throw "Could not find $Name. Install the Windows SDK or pass the tool path explicitly."
}

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

function New-ResizedPng {
    param(
        [string]$Source,
        [string]$Destination,
        [int]$Width,
        [int]$Height,
        [int]$ImageSize
    )

    Add-Type -AssemblyName System.Drawing
    $sourceImage = [System.Drawing.Image]::FromFile($Source)
    try {
        $bitmap = New-Object System.Drawing.Bitmap $Width, $Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $x = [int](($Width - $ImageSize) / 2)
                $y = [int](($Height - $ImageSize) / 2)
                $graphics.DrawImage($sourceImage, $x, $y, $ImageSize, $ImageSize)
            } finally {
                $graphics.Dispose()
            }
            $bitmap.Save($Destination, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally {
            $bitmap.Dispose()
        }
    } finally {
        $sourceImage.Dispose()
    }
}

$msixVersion = Convert-ToMsixVersion -Value $Version
$makeappx = Find-WindowsSdkTool -Name 'makeappx.exe' -RequestedPath $MakeAppxPath

if (-not $SkipBuild) {
    Assert-PathInside -Root $root -Path $buildRoot
    if (Test-Path -LiteralPath $buildRoot) {
        Remove-Item -LiteralPath $buildRoot -Recurse -Force
    }

    $buildArgs = @{
        Version = $Version
        Store = $true
        OutputDir = $buildRoot
    }
    & (Join-Path $root 'build.ps1') @buildArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
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
if (-not (Test-Path -LiteralPath $manifestTemplate)) {
    throw "Missing manifest template: $manifestTemplate"
}

New-Item -ItemType Directory -Force -Path $obj, $dist | Out-Null
Assert-PathInside -Root $root -Path $packageRoot
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $packageRoot, $assetsRoot | Out-Null

Copy-Item -LiteralPath $exe -Destination (Join-Path $packageRoot 'HdrSdrBrightness.exe')
Copy-Item -LiteralPath $captureDir -Destination (Join-Path $packageRoot 'capture') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $packageRoot 'LICENSE.txt')

$manifest = (Get-Content -LiteralPath $manifestTemplate -Raw).Replace('@VERSION4@', $msixVersion)
Set-Content -LiteralPath (Join-Path $packageRoot 'AppxManifest.xml') -Value $manifest -Encoding UTF8

$logo = Join-Path $root 'assets\app-logo-512.png'
if (-not (Test-Path -LiteralPath $logo)) {
    throw "Missing logo: $logo"
}
New-ResizedPng -Source $logo -Destination (Join-Path $assetsRoot 'StoreLogo.png') -Width 50 -Height 50 -ImageSize 44
New-ResizedPng -Source $logo -Destination (Join-Path $assetsRoot 'Square44x44Logo.png') -Width 44 -Height 44 -ImageSize 40
New-ResizedPng -Source $logo -Destination (Join-Path $assetsRoot 'Square150x150Logo.png') -Width 150 -Height 150 -ImageSize 116
New-ResizedPng -Source $logo -Destination (Join-Path $assetsRoot 'Square310x310Logo.png') -Width 310 -Height 310 -ImageSize 220
New-ResizedPng -Source $logo -Destination (Join-Path $assetsRoot 'Wide310x150Logo.png') -Width 310 -Height 150 -ImageSize 116

$msix = Join-Path $dist "HdrSdrBrightness-$Version-win64.msix"
$msixUpload = Join-Path $dist "HdrSdrBrightness-$Version-win64.msixupload"
Assert-PathInside -Root $root -Path $msix
Assert-PathInside -Root $root -Path $msixUpload
if (Test-Path -LiteralPath $msix) {
    Remove-Item -LiteralPath $msix -Force
}
if (Test-Path -LiteralPath $msixUpload) {
    Remove-Item -LiteralPath $msixUpload -Force
}

& $makeappx pack /d $packageRoot /p $msix /o
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not $SkipUpload) {
    Assert-PathInside -Root $root -Path $uploadRoot
    if (Test-Path -LiteralPath $uploadRoot) {
        Remove-Item -LiteralPath $uploadRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $uploadRoot | Out-Null

    Copy-Item -LiteralPath $msix -Destination (Join-Path $uploadRoot (Split-Path -Leaf $msix))
    if (-not [string]::IsNullOrWhiteSpace($AppxSymPath)) {
        if (-not (Test-Path -LiteralPath $AppxSymPath)) {
            throw "Missing app symbol package: $AppxSymPath"
        }
        if (-not [string]::Equals([IO.Path]::GetExtension($AppxSymPath), '.appxsym', [StringComparison]::OrdinalIgnoreCase)) {
            throw "App symbol package must use the .appxsym extension: $AppxSymPath"
        }
        Copy-Item -LiteralPath $AppxSymPath -Destination (Join-Path $uploadRoot (Split-Path -Leaf $AppxSymPath))
    }

    $uploadZip = "$msixUpload.zip"
    if (Test-Path -LiteralPath $uploadZip) {
        Remove-Item -LiteralPath $uploadZip -Force
    }
    $uploadFiles = @(Get-ChildItem -LiteralPath $uploadRoot -File | ForEach-Object { $_.FullName })
    Compress-Archive -LiteralPath $uploadFiles -DestinationPath $uploadZip -CompressionLevel Optimal
    Move-Item -LiteralPath $uploadZip -Destination $msixUpload
}

Write-Host "Created $msix"
if (-not $SkipUpload) {
    Write-Host "Created $msixUpload"
    Write-Host "Upload the MSIXUPLOAD to Partner Center only after a final Store test pass."
} else {
    Write-Host "Skipped MSIXUPLOAD creation."
}
Write-Host "For local sideload testing, sign the MSIX with a trusted certificate first."
