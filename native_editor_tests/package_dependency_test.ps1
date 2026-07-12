param(
    [Parameter(Mandatory = $true)]
    [string]$BuildRoot
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildScript = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'build.ps1')
$mainSource = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'src\main.cpp')
if ($buildScript -match '(?i)(dotnet|\.csproj|IncludeLegacyCSharp)' -or
    $mainSource -match '(?i)(CSharp|HdrSdrEditor\.exe|HdrSdrCapture\.exe)') {
    throw 'FAIL: managed build or runtime fallback remains reachable.'
}
$required = @(
    (Join-Path $BuildRoot 'HdrSdrBrightness.exe'),
    (Join-Path $BuildRoot 'capture\HdrSdrNativeCapture.exe'),
    (Join-Path $BuildRoot 'capture\HdrSdrNativeEditor.exe')
)
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path)) { throw "FAIL: missing native output $path" }
}

$managed = @(Get-ChildItem -LiteralPath $BuildRoot -Recurse -File | Where-Object {
    $_.Name -match '\.(dll|deps\.json|runtimeconfig\.json)$' -or
    $_.Name -in @('HdrSdrCapture.exe', 'HdrSdrEditor.exe')
})
if ($managed.Count -gt 0) {
    throw "FAIL: managed runtime artifacts remain: $($managed.Name -join ', ')"
}

$objdump = @(
    'C:\msys64\mingw64\bin\objdump.exe',
    'C:\msys64\ucrt64\bin\objdump.exe'
) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $objdump) { throw 'FAIL: objdump is required to verify native PE imports.' }
foreach ($path in $required) {
    $headers = (& $objdump -p $path) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw "FAIL: could not inspect PE imports for $path" }
    if ($headers -match '(?i)(hostfxr|coreclr|mscoree|hostpolicy)\.dll') {
        throw "FAIL: managed runtime import remains in $path"
    }
}
$editorHeaders = (& $objdump -p (Join-Path $BuildRoot 'capture\HdrSdrNativeEditor.exe')) -join "`n"
if ($editorHeaders -notmatch 'Subsystem\s+00000002\s+\(Windows GUI\)') {
    throw 'FAIL: native editor is not a GUI-subsystem executable.'
}
$editorPath = Join-Path $BuildRoot 'capture\HdrSdrNativeEditor.exe'
$versionInfo = (Get-Item -LiteralPath $editorPath).VersionInfo
if ($versionInfo.OriginalFilename -ne 'HdrSdrNativeEditor.exe' -or
    [string]::IsNullOrWhiteSpace($versionInfo.FileVersion)) {
    throw 'FAIL: native editor has no product/version resource.'
}
Add-Type -AssemblyName System.Drawing
$icon = [System.Drawing.Icon]::ExtractAssociatedIcon($editorPath)
if (-not $icon) { throw 'FAIL: native editor has no application icon.' }
$icon.Dispose()

$files = @(Get-ChildItem -LiteralPath $BuildRoot -Recurse -File)
$bytes = ($files | Measure-Object -Property Length -Sum).Sum
Write-Output "PASS: native-only build has $($files.Count) files and $bytes bytes; no .NET artifacts."
