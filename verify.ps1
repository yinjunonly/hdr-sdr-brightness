param(
    [switch]$SkipBuild,
    [switch]$SkipClipboardTests,
    [string]$DesktopBuildRoot,
    [string]$StoreBuildRoot,
    [int]$CaptureCount = 50
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($DesktopBuildRoot)) {
    $DesktopBuildRoot = Join-Path $root 'obj\validation\verify-desktop'
}
if ([string]::IsNullOrWhiteSpace($StoreBuildRoot)) {
    $StoreBuildRoot = Join-Path $root 'obj\validation\verify-store'
}

function Invoke-Checked {
    param([string]$Label, [scriptblock]$Command)
    Write-Host "== $Label =="
    & $Command
    if ($LASTEXITCODE -ne 0) { throw "$Label failed with exit code $LASTEXITCODE." }
}

if (-not $SkipBuild) {
    Invoke-Checked 'Desktop build' {
        & (Join-Path $root 'build.ps1') -OutputDir $DesktopBuildRoot
    }
    Invoke-Checked 'Store build' {
        & (Join-Path $root 'build.ps1') -Store -OutputDir $StoreBuildRoot
    }
}

$editor = Join-Path $DesktopBuildRoot 'capture\HdrSdrNativeEditor.exe'
$capture = Join-Path $DesktopBuildRoot 'capture\HdrSdrNativeCapture.exe'
$storeEditor = Join-Path $StoreBuildRoot 'capture\HdrSdrNativeEditor.exe'

Invoke-Checked 'Native image/editor unit tests' {
    & (Join-Path $root 'native_editor_tests\build.ps1')
}
Invoke-Checked 'Native capture unit tests' {
    & (Join-Path $root 'native_capture_tests\build.ps1')
}
Invoke-Checked 'First-run brightness initialization policy' {
    & (Join-Path $root 'brightness_tests\build.ps1')
}
Invoke-Checked 'First-run brightness production wiring' {
    & (Join-Path $root 'brightness_tests\initial_brightness_wiring_test.ps1')
}
Invoke-Checked 'Store startup policy unit tests' {
    & (Join-Path $root 'startup_tests\build.ps1')
}
Invoke-Checked 'Store startup manifest alias' {
    & (Join-Path $root 'startup_tests\store_manifest_alias_test.ps1')
}
Invoke-Checked 'Store fast startup wiring' {
    & (Join-Path $root 'startup_tests\store_fast_startup_wiring_test.ps1')
}
Invoke-Checked 'Per-Monitor V2 startup fallback' {
    & (Join-Path $root 'dpi_tests\build.ps1')
}
Invoke-Checked 'Desktop final executable DPI manifest' {
    & (Join-Path $root 'dpi_tests\final_executable_manifest_test.ps1') -BuildRoot $DesktopBuildRoot
}
Invoke-Checked 'Store final executable DPI manifest' {
    & (Join-Path $root 'dpi_tests\final_executable_manifest_test.ps1') -BuildRoot $StoreBuildRoot
}
Invoke-Checked 'Hidden warmup launch contract' {
    & (Join-Path $root 'native_capture_tests\background_server_launch_test.ps1')
}
Invoke-Checked 'Desktop native-only dependency gate' {
    & (Join-Path $root 'native_editor_tests\package_dependency_test.ps1') -BuildRoot $DesktopBuildRoot
}
Invoke-Checked 'Store native-only dependency gate' {
    & (Join-Path $root 'native_editor_tests\package_dependency_test.ps1') -BuildRoot $StoreBuildRoot
}
Invoke-Checked 'Tray-to-helper command adapter' {
    & (Join-Path $root 'native_editor_tests\capture_adapter_test.ps1') -BuildRoot $DesktopBuildRoot
}
Invoke-Checked 'Native capture pipe' {
    & (Join-Path $root 'native_capture_tests\capture_file_pipe_test.ps1') -NativeCapturePath $capture
}
Invoke-Checked 'Native capture resource lifetime' {
    & (Join-Path $root 'native_capture_tests\resource_lifetime_test.ps1') `
        -NativeCapturePath $capture -CaptureCount $CaptureCount
}
Invoke-Checked 'Latest native editor instance wins' {
    & (Join-Path $root 'native_editor_tests\editor_single_instance_test.ps1') -EditorPath $editor
}
Invoke-Checked 'Desktop editor smoke test' {
    & (Join-Path $root 'native_editor_tests\window_smoke_test.ps1') -EditorPath $editor
}
Invoke-Checked 'Store editor smoke test' {
    & (Join-Path $root 'native_editor_tests\window_smoke_test.ps1') -EditorPath $storeEditor
}
Invoke-Checked 'Preview maximize, restore, and mouse-wheel zoom' {
    & (Join-Path $root 'native_editor_tests\preview_window_interaction_test.ps1') -EditorPath $editor
}
Invoke-Checked 'Warm native capture-to-visible latency' {
    & (Join-Path $root 'native_editor_tests\native_pipeline_latency_test.ps1') `
        -EditorPath $editor -NativeCapturePath $capture -MaximumVisibleMs 250
}
Invoke-Checked '3440x1440 native region drag throughput' {
    & (Join-Path $root 'native_editor_tests\region_drag_performance_test.ps1') `
        -EditorPath $editor -FrameCount 120 -MinimumFramesPerSecond 55
}

if (-not $SkipClipboardTests) {
    Write-Warning 'The next two integration tests temporarily replace the current clipboard image.'
    Invoke-Checked 'Physical-pixel region selection and clipboard' {
        & (Join-Path $root 'native_editor_tests\region_selection_test.ps1') -EditorPath $editor
    }
    Invoke-Checked 'Live and committed mosaic brush pixels' {
        & (Join-Path $root 'native_editor_tests\region_mosaic_brush_test.ps1') -EditorPath $editor
    }
    Invoke-Checked 'Preview tools, history, options, and clipboard' {
        & (Join-Path $root 'native_editor_tests\preview_edit_test.ps1') -EditorPath $editor
    }
    Invoke-Checked 'Native Save As dialog' {
        & (Join-Path $root 'native_editor_tests\save_dialog_test.ps1') -EditorPath $editor
    }
}

$leftovers = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -in @('HdrSdrNativeEditor.exe', 'HdrSdrNativeCapture.exe') -and
    $_.ExecutablePath -like '*\obj\validation\*'
})
if ($leftovers.Count -gt 0) {
    throw "Validation left helper processes running: $($leftovers.ProcessId -join ', ')"
}

Write-Host 'PASS: pure C++ desktop and Store verification completed without packaging or publishing.'
