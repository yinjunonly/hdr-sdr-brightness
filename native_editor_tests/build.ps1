param(
    [string]$OutputDir
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root 'obj\native-editor-tests'
}

function Resolve-BuildTool {
    param([string]$Name, [string[]]$FallbackPaths)
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    foreach ($path in $FallbackPaths) {
        if (Test-Path -LiteralPath $path) { return $path }
    }
    throw "Missing build tool: $Name"
}

$gpp = Resolve-BuildTool -Name 'g++' -FallbackPaths @(
    'C:\msys64\mingw64\bin\g++.exe',
    'C:\msys64\ucrt64\bin\g++.exe'
)
$toolDir = Split-Path -Parent $gpp
if ($toolDir -and ($env:PATH -notlike "*$toolDir*")) {
    $env:PATH = $toolDir + [IO.Path]::PathSeparator + $env:PATH
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$exe = Join-Path $OutputDir 'image_document_test.exe'
$sources = @(
    (Join-Path $PSScriptRoot 'image_document_test.cpp'),
    (Join-Path $root 'native_editor\bmp_codec.cpp'),
    (Join-Path $root 'native_editor\image_document.cpp'),
    (Join-Path $root 'native_editor\annotation_renderer.cpp'),
    (Join-Path $root 'native_editor\mosaic_renderer.cpp')
)

& $gpp $sources `
    -std=gnu++17 `
    -DWIN32_LEAN_AND_MEAN `
    -DNOMINMAX `
    -O2 `
    -Wall `
    -Wextra `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -s `
    -o $exe `
    -lgdiplus
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$outputExe = Join-Path $OutputDir 'output_test.exe'
$outputSources = @(
    (Join-Path $PSScriptRoot 'output_test.cpp'),
    (Join-Path $root 'native_editor\image_document.cpp'),
    (Join-Path $root 'native_editor\annotation_renderer.cpp'),
    (Join-Path $root 'native_editor\mosaic_renderer.cpp'),
    (Join-Path $root 'native_editor\wic_png.cpp')
)
& $gpp $outputSources `
    -std=gnu++17 `
    -DWIN32_LEAN_AND_MEAN `
    -DNOMINMAX `
    -O2 `
    -Wall `
    -Wextra `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -s `
    -o $outputExe `
    -lgdiplus `
    -lole32 `
    -lwindowscodecs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $outputExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$toolbarExe = Join-Path $OutputDir 'toolbar_layout_test.exe'
$toolbarSources = @(
    (Join-Path $PSScriptRoot 'toolbar_layout_test.cpp'),
    (Join-Path $root 'native_editor\editor_toolbar.cpp'),
    (Join-Path $root 'native_editor\editor_icons.cpp')
)
& $gpp $toolbarSources `
    -std=gnu++17 `
    -DWIN32_LEAN_AND_MEAN `
    -DNOMINMAX `
    -O2 `
    -Wall `
    -Wextra `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -s `
    -o $toolbarExe `
    -lgdiplus
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $toolbarExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$localizationExe = Join-Path $OutputDir 'localization_test.exe'
& $gpp (Join-Path $PSScriptRoot 'localization_test.cpp') `
    (Join-Path $root 'native_editor\editor_text.cpp') `
    -std=gnu++17 `
    -O2 `
    -Wall `
    -Wextra `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -s `
    -o $localizationExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $localizationExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$surfaceExe = Join-Path $OutputDir 'dib_surface_test.exe'
& $gpp (Join-Path $PSScriptRoot 'dib_surface_test.cpp') `
    (Join-Path $root 'native_editor\dib_surface.cpp') `
    (Join-Path $root 'native_editor\image_document.cpp') `
    (Join-Path $root 'native_editor\annotation_renderer.cpp') `
    (Join-Path $root 'native_editor\mosaic_renderer.cpp') `
    -std=gnu++17 `
    -DWIN32_LEAN_AND_MEAN `
    -DNOMINMAX `
    -O2 `
    -Wall `
    -Wextra `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -s `
    -o $surfaceExe `
    -lgdiplus `
    -lgdi32 `
    -luser32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $surfaceExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$captureQueueExe = Join-Path $OutputDir 'capture_request_queue_test.exe'
& $gpp (Join-Path $PSScriptRoot 'capture_request_queue_test.cpp') `
    (Join-Path $root 'src\capture_request_queue.cpp') `
    -std=gnu++17 `
    -O2 `
    -Wall `
    -Wextra `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -s `
    -o $captureQueueExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $captureQueueExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$windowControlExe = Join-Path $OutputDir 'editor_window_control_test.exe'
& $gpp (Join-Path $PSScriptRoot 'editor_window_control_test.cpp') `
    (Join-Path $root 'src\editor_window_control.cpp') `
    -std=gnu++17 `
    -DWIN32_LEAN_AND_MEAN `
    -DNOMINMAX `
    -O2 `
    -Wall `
    -Wextra `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -s `
    -o $windowControlExe `
    -luser32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $windowControlExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$viewportExe = Join-Path $OutputDir 'preview_viewport_test.exe'
& $gpp (Join-Path $PSScriptRoot 'preview_viewport_test.cpp') `
    (Join-Path $root 'native_editor\preview_viewport.cpp') `
    -std=gnu++17 `
    -DWIN32_LEAN_AND_MEAN `
    -DNOMINMAX `
    -O2 `
    -Wall `
    -Wextra `
    -static `
    -static-libgcc `
    -static-libstdc++ `
    -s `
    -o $viewportExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $viewportExe
exit $LASTEXITCODE
