$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$manifestPath = Join-Path $root 'store\AppxManifest.xml.in'
[xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw

$namespaces = New-Object System.Xml.XmlNamespaceManager($manifest.NameTable)
$namespaces.AddNamespace('f', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
$namespaces.AddNamespace('uap3', 'http://schemas.microsoft.com/appx/manifest/uap/windows10/3')
$namespaces.AddNamespace('desktop', 'http://schemas.microsoft.com/appx/manifest/desktop/windows10')

$alias = $manifest.SelectSingleNode(
    '//uap3:Extension[@Category="windows.appExecutionAlias"]/uap3:AppExecutionAlias/desktop:ExecutionAlias',
    $namespaces)
if (-not $alias) {
    throw 'FAIL: Store manifest does not register a stable app execution alias.'
}
if ($alias.Alias -ne 'HdrSdrBrightnessStore.exe') {
    throw "FAIL: unexpected Store app execution alias: $($alias.Alias)"
}

Write-Output 'PASS: Store manifest registers the stable fast-startup execution alias.'
