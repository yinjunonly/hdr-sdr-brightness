$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$docsRoot = Join-Path $projectRoot 'docs'
$brandAsset = 'assets/app-logo-256.png'
$brandAssetPath = Join-Path $docsRoot ($brandAsset -replace '/', [IO.Path]::DirectorySeparatorChar)
$pages = @(
    'index.html',
    'zh-cn.html',
    'hdr-screenshot-washed-out.html'
)

if (-not (Test-Path -LiteralPath $brandAssetPath -PathType Leaf)) {
    throw "Published brand asset is missing: $brandAssetPath"
}

$signature = [IO.File]::ReadAllBytes($brandAssetPath)[0..7]
$expectedPngSignature = [byte[]](137, 80, 78, 71, 13, 10, 26, 10)
if (Compare-Object $expectedPngSignature $signature) {
    throw "Published brand asset is not a valid PNG: $brandAssetPath"
}

foreach ($page in $pages) {
    $pagePath = Join-Path $docsRoot $page
    $html = Get-Content -Raw -LiteralPath $pagePath

    if ($html -match '<span\s+class="mark"') {
        throw "$page still renders the CSS placeholder mark"
    }

    if ($html -notmatch ('<img\s+class="mark"\s+src="' + [regex]::Escape($brandAsset) + '"\s+alt=""')) {
        throw "$page does not render the published application logo"
    }

    if ($html -notmatch ('<link\s+rel="icon"\s+type="image/png"\s+href="' + [regex]::Escape($brandAsset) + '"')) {
        throw "$page does not use the application logo as its favicon"
    }
}

Write-Output 'GitHub Pages branding validation passed.'
