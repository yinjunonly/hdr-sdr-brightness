# Native C++ Capture Editor Implementation Plan

> **For agentic workers:** Execute inline in the current dirty workspace so the unreleased 1.1.0 work remains available. Track each test-first slice independently and do not commit, package, or publish without explicit user approval.

**Goal:** Ship a feature-equivalent native C++ screenshot editor and remove the application package's .NET dependency.

**Architecture:** Keep WGC/D3D11 capture in the persistent native capture server and introduce a short-lived `HdrSdrNativeEditor.exe` for Win32/GDI+/WIC editing. Switch the tray app only after native region and fullscreen paths pass parity tests.

**Tech Stack:** C++17, Win32, GDI+, WIC, COM, WGC, D3D11, PowerShell integration tests, MinGW-w64.

## Implementation Status - 2026-07-12

Tasks 1-8 are implemented in the dirty working tree. The automated portion of Task 9 passes for isolated desktop and Store builds: native-only dependency gates, WGC pipe/lifetime checks, physical-pixel region selection, preview editing, clipboard PNG/CF_DIB, Save-dialog wiring, editor smoke tests, localization, and ten sub-250-ms capture-to-visible runs. No package or release was created.

Still manual before release: exercise the real global hotkey from the isolated full app, confirm one Save As operation, visually inspect 100%/150%/200% and mixed-DPI displays, inspect all seven languages for clipping, and smoke-test an installed MSIX after explicit packaging approval. The detailed current state and validation numbers are recorded in `AI_HANDOFF.md`.

## Global Constraints

- Preserve every capability listed in `docs/superpowers/specs/2026-07-11-native-cpp-capture-editor-design.md`.
- Minimum OS remains Windows 10 1809; architecture remains x64.
- Use only Windows system libraries at runtime.
- Keep image/document coordinates in physical pixels.
- Do not publish, package, upload, submit, push, tag, or commit without explicit user instruction.
- Update `AI_HANDOFF.md` after every code change.

---

### Task 1: Close Every WGC Capture Lifetime

**Files:**
- Create: `native_capture_tests/resource_lifetime_test.ps1`
- Modify: `native_capture/native_capture_backend.cpp`
- Modify: `native_capture/native_common.h`
- Modify: `AI_HANDOFF.md`

**Interfaces:**
- Produces: `CloseWinRtObject(IInspectable*)`, which queries `Windows.Foundation.IClosable` and invokes `Close()`.
- Verifies: 20 repeated public `capture-file` pipe commands do not accumulate threads or handles.

- [ ] Add a process-level regression test that starts `HdrSdrNativeCapture --server`, records process resources after one warm capture, performs 20 additional captures, and fails when thread delta exceeds 2 or handle delta exceeds 20.
- [ ] Run the test against the current binary and record the expected failure.
- [ ] Add an RAII close helper for WinRT closable objects.
- [ ] Close the capture session before the frame pool on every success, timeout, and error return from `CaptureOneFrame`.
- [ ] Rebuild the native helper and rerun the lifetime test until it passes.

### Task 2: Native Physical-Pixel Image Core

**Files:**
- Create: `native_editor/image_document.h`
- Create: `native_editor/image_document.cpp`
- Create: `native_editor/bmp_codec.h`
- Create: `native_editor/bmp_codec.cpp`
- Create: `native_editor_tests/image_document_test.cpp`
- Create: `native_editor_tests/build.ps1`
- Modify: `AI_HANDOFF.md`

**Interfaces:**
- Produces: `editor::BgraImage { UINT width; UINT height; std::vector<BYTE> pixels; }`.
- Produces: `bool LoadTopDownBmp(const std::wstring&, BgraImage*, std::wstring*)`.
- Produces: `ImageDocument::Render()` and edit-history operations.

- [ ] Add failing tests for top-down 32-bit BMP bottom-row preservation, truncated input rejection, crop mapping, adjustment presets, undo, redo, and reset.
- [ ] Implement strict BMP parsing with checked sizes and opaque alpha normalization.
- [ ] Implement immutable source storage and deterministic render history.
- [ ] Port Low/Balanced/High preset math byte-for-byte from `CaptureImageEditing.cs`.
- [ ] Run the native image tests.

### Task 3: Native PNG And Clipboard Output

**Files:**
- Create: `native_editor/wic_png.h`
- Create: `native_editor/wic_png.cpp`
- Create: `native_editor/editor_clipboard.h`
- Create: `native_editor/editor_clipboard.cpp`
- Create: `native_editor_tests/output_test.cpp`
- Modify: `native_editor_tests/build.ps1`
- Modify: `AI_HANDOFF.md`

**Interfaces:**
- Produces: `EncodePng(const BgraImage&, std::vector<BYTE>*, std::wstring*)`.
- Produces: `SavePng(const std::wstring&, const BgraImage&, std::wstring*)`.
- Produces: `CopyImageToClipboard(HWND, const BgraImage&, std::wstring*)` with PNG and CF_DIB.

- [ ] Add a failing PNG signature/dimension round-trip test.
- [ ] Implement WIC PNG encoding to `IStream` and file output.
- [ ] Add a clipboard integration test that validates both registered PNG and CF_DIB formats.
- [ ] Implement retrying clipboard ownership transfer with correct `HGLOBAL` lifetime.
- [ ] Run output tests in the interactive user session.

### Task 4: Native Editor Process And Preview Window

**Files:**
- Create: `native_editor/native_editor.cpp`
- Create: `native_editor/editor_options.h`
- Create: `native_editor/editor_options.cpp`
- Create: `native_editor/editor_window.h`
- Create: `native_editor/editor_window.cpp`
- Create: `native_editor/build.ps1`
- Create: `native_editor_tests/window_smoke_test.ps1`
- Modify: `AI_HANDOFF.md`

**Interfaces:**
- Supports: `--select-file`, `--edit-file`, `--output`, and `--lang` command-line modes.
- Produces: a Per-Monitor-V2 borderless native preview window with explicit physical-pixel transforms.

- [ ] Add a smoke test that launches an image fixture, finds one visible editor window, verifies its process is native-only, and closes it with `Esc`.
- [ ] Implement command-line parsing and COM/GDI+ startup/shutdown.
- [ ] Implement preview fit, borderless title bar, resize hit testing, window dragging, dark DWM attributes, and close behavior.
- [ ] Verify physical pixels remain sharp at 100%, 125%, 150%, and 200% DPI.

### Task 5: Region Selection And Toolbar

**Files:**
- Create: `native_editor/region_selection.h`
- Create: `native_editor/region_selection.cpp`
- Create: `native_editor/editor_toolbar.h`
- Create: `native_editor/editor_toolbar.cpp`
- Create: `native_editor/editor_icons.h`
- Create: `native_editor/editor_icons.cpp`
- Create: `native_editor_tests/region_selection_test.ps1`
- Modify: `AI_HANDOFF.md`

**Interfaces:**
- Produces: persistent selection geometry, toolbar hit testing, options popover hit testing, and Copy/Save commit actions.

- [ ] Add an integration test that drags a physical selection and verifies the selected output dimensions and four edge pixels.
- [ ] Keep selection active after mouse-up and place the 14-action toolbar above/below it using the current clamping rules.
- [ ] Port icons, hover/pressed states, tooltips, color swatches, and size popovers.
- [ ] Verify Esc cancels immediately and focus returns to the previous foreground window.

### Task 6: Native Annotation Engine

**Files:**
- Create: `native_editor/edit_operation.h`
- Create: `native_editor/annotation_renderer.h`
- Create: `native_editor/annotation_renderer.cpp`
- Create: `native_editor/mosaic_renderer.h`
- Create: `native_editor/mosaic_renderer.cpp`
- Create: `native_editor_tests/annotation_test.cpp`
- Modify: `AI_HANDOFF.md`

**Interfaces:**
- Produces: Rectangle, Ellipse, Pen, and Mosaic edit operations in physical image coordinates.
- Produces: deterministic rebuild from source, preset, and ordered operations.

- [ ] Add pixel-level tests for rectangle, ellipse bounds, segmented pen paths, mosaic region/brush, and clipping.
- [ ] Port six colors and three sizes per tool.
- [ ] Implement undo, redo, reset, `Ctrl+Z`, and `Ctrl+Y` through the document model.
- [ ] Compare native output with managed reference fixtures within defined antialiasing tolerance.

### Task 7: Localization And Fullscreen Editing Parity

**Files:**
- Create: `native_editor/editor_text.h`
- Create: `native_editor/editor_text.cpp`
- Create: `native_editor_tests/localization_test.cpp`
- Modify: `native_editor/editor_window.cpp`
- Modify: `AI_HANDOFF.md`

**Interfaces:**
- Produces: localized editor strings for language IDs already passed by the tray app.

- [ ] Port every `CaptureString` value for all seven shipped languages.
- [ ] Add a completeness test that rejects missing or empty translations.
- [ ] Complete fullscreen preview Copy, Save As, status feedback, annotation, adjustment, and keyboard parity.
- [ ] Visually inspect all languages for clipping at supported DPI values.

### Task 8: Switch Main App And Remove .NET From Shipping

**Files:**
- Modify: `src/fullscreen_capture_adapter.cpp`
- Modify: `src/fullscreen_capture_adapter.h`
- Modify: `src/main.cpp`
- Modify: `build.ps1`
- Modify: `package.ps1`
- Modify: `package-msix.ps1`
- Modify: `README.md`
- Modify: `store/listing-draft.md`
- Create: `native_editor_tests/package_dependency_test.ps1`
- Modify: `AI_HANDOFF.md`

**Interfaces:**
- Main launches `HdrSdrNativeEditor.exe` for both selection and edit modes.
- Shipping output contains only native executables plus project documents.

- [ ] Add a package-layout test that fails while managed editor files or runtime configuration remain.
- [ ] Build and include the native editor in desktop and Store output.
- [ ] Switch warm region and fullscreen edit command builders to the native editor.
- [ ] Stop prewarming `HdrSdrEditor`; remove `dotnet publish` from the default and Store builds.
- [ ] Remove managed editor artifacts from package checks and user-facing dependency claims.
- [ ] Rebuild from a clean source checkout-equivalent file list and pass the package-layout test.

### Task 9: Release-Candidate Verification

**Files:**
- Create: `verify.ps1`
- Modify: `AI_HANDOFF.md`

**Interfaces:**
- Produces: one non-packaging command that runs builds, unit tests, UI tests, performance checks, resource soak, and dependency checks.

- [ ] Run desktop and Store isolated builds.
- [ ] Run tone-map, image-core, annotation, output, DPI, clipboard, and lifecycle tests.
- [ ] Run 50 repeated captures and 20 editor open/close cycles.
- [ ] Measure hotkey-to-visible latency and idle process resources.
- [ ] Manually validate region/fullscreen Copy, Save, Cancel, each annotation tool, presets, undo/redo, all languages, 100%/125%/150%/200% DPI, and mixed-DPI dual monitors.
- [ ] Confirm no .NET process, assembly, runtime config, console window, clipping, overexposure, or resolution loss remains.
