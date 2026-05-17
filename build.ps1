param(
    [switch]$Clean,
    [string]$Version
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$bin = Join-Path $root 'bin'
$obj = Join-Path $root 'obj'
$sources = @(
    (Join-Path $root 'src\main.cpp'),
    (Join-Path $root 'src\localization.cpp')
)
$res = Join-Path $root 'res\app.rc'
$resObj = Join-Path $obj 'app.res.o'
$versionFile = Join-Path $root 'VERSION'
$versionHeader = Join-Path $obj 'version.h'
$manifestTemplate = Join-Path $root 'res\app.manifest.in'
$manifest = Join-Path $obj 'app.manifest'
$exe = Join-Path $bin 'HdrSdrBrightness.exe'

function Get-ProjectVersion {
    param(
        [string]$Root,
        [string]$RequestedVersion
    )

    if ([string]::IsNullOrWhiteSpace($RequestedVersion)) {
        $path = Join-Path $Root 'VERSION'
        if (-not (Test-Path -LiteralPath $path)) {
            throw "Missing version file: $path"
        }
        $RequestedVersion = (Get-Content -LiteralPath $path -Raw).Trim()
    }

    if ($RequestedVersion -notmatch '^\d+\.\d+\.\d+$') {
        throw "Version must use MAJOR.MINOR.PATCH, for example 1.0.0"
    }

    return $RequestedVersion
}

function Convert-ToResourceVersion {
    param([string]$Version)

    $parts = @($Version.Split('.')) + @('0')
    foreach ($part in $parts) {
        $value = [int]$part
        if ($value -lt 0 -or $value -gt 65535) {
            throw "Version part out of range for Windows resources: $part"
        }
    }

    return $parts
}

$Version = Get-ProjectVersion -Root $root -RequestedVersion $Version
$versionParts = Convert-ToResourceVersion -Version $Version
$version4 = $versionParts -join '.'

if ($Clean -and (Test-Path $bin)) {
    $resolvedBin = (Resolve-Path -LiteralPath $bin).Path
    if (-not $resolvedBin.StartsWith($root + '\')) {
        throw "Refusing to clean outside project: $resolvedBin"
    }
    Remove-Item -LiteralPath $resolvedBin -Recurse -Force
}
if ($Clean -and (Test-Path $obj)) {
    $resolvedObj = (Resolve-Path -LiteralPath $obj).Path
    if (-not $resolvedObj.StartsWith($root + '\')) {
        throw "Refusing to clean outside project: $resolvedObj"
    }
    Remove-Item -LiteralPath $resolvedObj -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $bin | Out-Null
New-Item -ItemType Directory -Force -Path $obj | Out-Null

if (-not (Test-Path -LiteralPath $manifestTemplate)) {
    throw "Missing manifest template: $manifestTemplate"
}

$gpp = (Get-Command g++ -ErrorAction Stop).Source
$windres = (Get-Command windres -ErrorAction Stop).Source

$icon = Join-Path $root 'assets\app.ico'
if (-not (Test-Path -LiteralPath $icon)) {
    throw "Missing icon: $icon"
}

Push-Location $root
try {
$versionHeaderContent = @"
#pragma once
#define APP_VERSION "$Version"
#define APP_VERSION_W L"$Version"
#define APP_VERSION_COMMA $($versionParts -join ',')
"@
    Set-Content -LiteralPath $versionHeader -Value $versionHeaderContent -Encoding ASCII

    $manifestContent = (Get-Content -LiteralPath $manifestTemplate -Raw).Replace('@VERSION4@', $version4)
    Set-Content -LiteralPath $manifest -Value $manifestContent -Encoding UTF8

    & $windres --codepage=65001 -I $root $res -O coff -o $resObj
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & $gpp $sources $resObj `
        -std=gnu++11 `
        -O2 `
        -mwindows `
        -I $obj `
        -DUNICODE `
        -D_UNICODE `
        -Wall `
        -Wextra `
        -Wno-missing-field-initializers `
        -static-libgcc `
        -static-libstdc++ `
        -o $exe `
        -luser32 `
        -lshell32 `
        -lgdi32 `
        -lgdiplus `
        -ladvapi32

    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}

Write-Host "Built $exe (version $Version)"
