param(
    [switch]$AllowDirtySource,
    [switch]$WiringOnly
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$requiredWiring = @(
    @{
        Path = Join-Path $root 'verify.ps1'
        Pattern = 'app_tests\build.ps1'
        Description = 'verify.ps1 must run the app-state regression suite'
    },
    @{
        Path = Join-Path $root 'package.ps1'
        Pattern = 'release-preflight.ps1'
        Description = 'package.ps1 must run release preflight'
    },
    @{
        Path = Join-Path $root 'package-msix.ps1'
        Pattern = 'release-preflight.ps1'
        Description = 'package-msix.ps1 must run release preflight'
    }
)

foreach ($requirement in $requiredWiring) {
    $source = Get-Content -LiteralPath $requirement.Path -Raw
    if (-not $source.Contains($requirement.Pattern)) {
        throw $requirement.Description
    }
}

Write-Host 'PASS: required release validation wiring is present.'
if ($WiringOnly) {
    exit 0
}

if (-not $AllowDirtySource) {
    $trackedChanges = @(& git -C $root status --porcelain --untracked-files=no)
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not inspect the Git worktree.'
    }
    if ($trackedChanges.Count -ne 0) {
        throw 'Formal packaging requires a clean tracked Git worktree. Use -AllowDirtySource only for local prerelease testing.'
    }
}

& (Join-Path $root 'verify.ps1') -SkipClipboardTests
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host 'PASS: release preflight validation completed.'
