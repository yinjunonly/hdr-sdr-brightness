$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$manifestPath = Join-Path $root 'store\AppxManifest.xml.in'
[xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw

$namespaces = New-Object System.Xml.XmlNamespaceManager($manifest.NameTable)
$namespaces.AddNamespace('f', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
$namespaces.AddNamespace('uap3', 'http://schemas.microsoft.com/appx/manifest/uap/windows10/3')
$namespaces.AddNamespace('desktop', 'http://schemas.microsoft.com/appx/manifest/desktop/windows10')

$startupTask = $manifest.SelectSingleNode(
    '//desktop:Extension[@Category="windows.startupTask"]/desktop:StartupTask',
    $namespaces)
if (-not $startupTask) {
    throw 'FAIL: Store manifest must retain the Windows-managed startup task.'
}

$alias = $manifest.SelectSingleNode(
    '//uap3:Extension[@Category="windows.appExecutionAlias"]/uap3:AppExecutionAlias/desktop:ExecutionAlias',
    $namespaces)
if ($alias) {
    throw "FAIL: Store manifest must not register the fast-startup execution alias: $($alias.Alias)"
}

Write-Output 'PASS: Store manifest uses only the Windows-managed startup task.'
