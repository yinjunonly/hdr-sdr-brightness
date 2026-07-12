# Native C++ Capture Editor Design

## Goal

Replace the shipped .NET/WinForms screenshot editor with a fully native C++ implementation while preserving the existing capture, selection, annotation, clipboard, save, localization, DPI, and keyboard behavior.

The final desktop ZIP and Store MSIX must not contain `HdrSdrEditor.exe`, managed DLLs, `.deps.json`, `.runtimeconfig.json`, or require a .NET Desktop Runtime.

## Supported Platform

- Windows 10 version 1809 (`10.0.17763.0`) or newer.
- Windows 11.
- x64 only for 1.1.0.
- No administrator privileges, driver, service, account, or network connection.

## Process Architecture

### `HdrSdrBrightness.exe`

Owns tray behavior, brightness automation, settings, global hotkeys, process startup, and user notifications. It does not own large screenshot pixel buffers.

### `HdrSdrNativeCapture.exe`

Remains the long-lived WGC/D3D11 capture server. It owns the reusable capture item, D3D device/context, HDR readback, tone mapping, and temporary physical-resolution BGRA output.

Every per-capture `GraphicsCaptureSession` and `Direct3D11CaptureFramePool` is explicitly closed through `Windows.Foundation.IClosable` before its COM references are released.

### `HdrSdrNativeEditor.exe`

New short-lived native process. The tray app runs one hidden `--warmup` invocation after startup to populate system code pages, then every visible editor starts on demand without remaining resident. It owns:

- Exact physical-pixel BMP loading.
- Region selection and fullscreen preview/edit windows.
- Vector edit history and redo history.
- Annotation rendering and mosaic processing.
- WIC PNG encoding and Save As UI.
- PNG plus CF_DIB clipboard publishing.
- Capture-editor localization and keyboard input.

Keeping this process separate prevents large image allocations or an editor crash from destabilizing the tray process or the warm capture server.

## Native Technology

- Win32 windowing, input, focus, DPI, menus, dialogs, and clipboard.
- GDI/GDI+ for custom UI and antialiased annotations.
- Windows Imaging Component for PNG encoding.
- Existing WGC/D3D11 native capture and tone mapping.
- System DLLs only; no newly bundled runtime.

## Functional Parity

The native editor must preserve:

- Physical-resolution primary-monitor region selection.
- Fullscreen preview editor.
- Rectangle marker, ellipse, freehand pen, and mosaic brush/region.
- Six annotation colors.
- Three sizes for every annotation tool.
- Low, Balanced, and High adjustment presets.
- Undo, redo, reset, `Ctrl+Z`, `Ctrl+Y`, and `Esc`.
- Copy and Save actions.
- PNG and CF_DIB clipboard formats.
- PNG Save As dialog.
- Simplified Chinese, Traditional Chinese, English, Korean, Japanese, Russian, and German.
- Dark custom-drawn UI, toolbar feedback, tooltips, DPI-aware geometry, and correct icon margins.

The migration is not an opportunity to redesign the toolbar. Existing layout, ordering, colors, and interaction remain the visual reference unless a native platform constraint requires a measured adjustment.

## Image Model

`ImageDocument` stores an immutable source BGRA image, current adjustment preset, ordered edit operations, and redo operations. The rendered bitmap is rebuilt deterministically from source plus operations. This keeps undo/redo behavior equivalent to the WinForms editor and prevents cumulative quality loss.

Coordinates in the document model are physical image pixels. Window/client coordinates are converted only at the UI boundary.

## Data Flow

### Region screenshot

1. Main process sends `capture-file` to the warm native capture server.
2. Capture server writes a top-down 32-bit physical-resolution BMP.
3. Main launches `HdrSdrNativeEditor.exe --select-file ...`.
4. Editor loads the BMP without GDI DPI conversion.
5. User selects, annotates, adjusts, and copies or saves.

### Fullscreen screenshot

1. Main sends `fullscreen-clip` to the warm native capture server.
2. The fullscreen image is copied immediately as today.
3. The notification Edit action launches `HdrSdrNativeEditor.exe --edit-file ...`.

## Failure Behavior

- Capture failure leaves the existing localized tray notification path intact.
- Editor launch failure leaves the captured BMP on disk and reports a localized launch error.
- Clipboard contention retries before reporting a status error.
- Save cancellation is not an error.
- Invalid/truncated BMP input is rejected without reading beyond file bounds.

## Performance And Resource Gates

- Warm region hotkey to visible editor: no slower than 250 ms on the current 3440x1440 validation machine.
- Editor process is not resident while idle.
- Fifty warm captures may not increase native-server thread count by more than 2 or handle count by more than 20 after the first warm capture.
- Idle helper CPU remains effectively zero.
- Output dimensions and edge pixels remain identical to the selected physical source region.

## Migration Gates

The main process switches only after native region and fullscreen vertical slices pass their integration tests. The shipping build must contain exactly the native tray app, native capture helper, and native editor; it must have no managed fallback or optional .NET publishing path.

No packaging, release, upload, or Store submission is part of implementation work until the user explicitly requests release work.
