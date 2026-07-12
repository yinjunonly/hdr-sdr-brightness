# Capture Replacement And Preview Zoom Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Coalesce repeated interactive screenshot requests to the newest capture and add a maximizable, wheel-zoomable native fullscreen preview.

**Architecture:** A pure latest-request coordinator keeps capture scheduling deterministic on the tray UI thread, while editor-window control closes superseded native windows. A standalone preview viewport owns fit/zoom/pan geometry and keeps annotation coordinates in physical image pixels.

**Tech Stack:** Win32 C++17, GDI/GDI+, PowerShell integration tests, MinGW.

## Global Constraints

- A repeated hotkey replaces the current screenshot; it does not reactivate or preserve it.
- Rapid presses coalesce to the last request and never fan out into multiple editor windows.
- Preview zoom is pointer-centered, ranges from fit (`1x`) to `8x`, and supports middle-button pan.
- Desktop and Store outputs remain three native executables with no .NET dependency.
- Do not commit, package, publish, or submit without a separate explicit user request.

---

### Task 1: Latest-request capture scheduling

**Files:**
- Create: `src/capture_request_queue.h`
- Create: `src/capture_request_queue.cpp`
- Create: `native_editor_tests/capture_request_queue_test.cpp`
- Modify: `native_editor_tests/build.ps1`

**Interfaces:**
- Produces: `capture_request::Queue::Request()` and `capture_request::Queue::Complete(uint64_t)` decisions consumed by `src/main.cpp`.

- [ ] Write tests proving the first request starts, rapid requests coalesce, stale completion starts only the newest generation, and only the current completion may show a result.
- [ ] Run `powershell -ExecutionPolicy Bypass -File .\native_editor_tests\build.ps1` and verify the new test fails because the queue does not exist.
- [ ] Implement the minimal UI-thread-owned queue state machine.
- [ ] Re-run the native editor test build and verify all unit tests pass.

### Task 2: Region hotkey replacement

**Files:**
- Create: `src/editor_window_control.h`
- Create: `src/editor_window_control.cpp`
- Modify: `src/main.cpp`
- Modify: `build.ps1`

**Interfaces:**
- Consumes: `capture_request::Queue`.
- Produces: `editor_window_control::CloseAll()` and a `WM_APP` completion path that launches only the newest successful region capture.

- [ ] Add a focused window-control test that creates windows with both native editor class names and expects `CloseAll()` to send `WM_CLOSE` to each.
- [ ] Run the test and verify it fails before implementation.
- [ ] Move region editor launch from the capture worker to the tray window completion handler.
- [ ] Close existing editor windows on every request and again before launching the latest result.
- [ ] Feed worker-start and capture failures through the same latest-request completion path.
- [ ] Re-run the queue/window tests and manually stress the built tray app with rapid `WM_HOTKEY` messages, asserting no more than one native editor window remains.

### Task 3: Physical-pixel preview viewport

**Files:**
- Create: `native_editor/preview_viewport.h`
- Create: `native_editor/preview_viewport.cpp`
- Create: `native_editor_tests/preview_viewport_test.cpp`
- Modify: `native_editor_tests/build.ps1`
- Modify: `native_editor/build.ps1`

**Interfaces:**
- Produces: fit layout, `ZoomAt`, `PanBy`, `ClientToImage`, and `ImageToClient` operations used by `preview_editor.cpp`.

- [ ] Write unit tests for fit layout, `1x`/`8x` limits, pointer anchoring, panning, blank-space clamping, and coordinate round trips.
- [ ] Run the test and verify it fails before implementation.
- [ ] Implement the viewport model without rendering or window dependencies beyond Win32 geometry types.
- [ ] Re-run the viewport and existing image/editor tests.

### Task 4: Maximize and wheel interaction

**Files:**
- Modify: `native_editor/preview_editor.cpp`
- Create: `native_editor_tests/preview_window_interaction_test.ps1`
- Modify: `verify.ps1`

**Interfaces:**
- Consumes: `PreviewViewport`.
- Produces: a custom maximize/restore title button, work-area-aware maximization, `WM_MOUSEWHEEL` zoom, and middle-button pan.

- [ ] Add an integration test that opens a real preview, clicks the custom title button, verifies `IsZoomed`, clicks again, and verifies restore.
- [ ] Run the test and verify it fails because the current title bar has only Close.
- [ ] Replace direct `FitImage` calculations with `PreviewViewport` and route all annotation coordinate transforms through it.
- [ ] Handle wheel zoom and middle-button pan while suppressing viewport changes during annotation strokes.
- [ ] Draw and hit-test maximize/restore immediately left of Close and constrain maximized size to the monitor work area.
- [ ] Run the new interaction test plus preview edit, Save dialog, toolbar, and DPI region tests.

### Task 5: Full validation and local handoff

**Files:**
- Modify: `AI_HANDOFF.md`

**Interfaces:**
- Consumes: all preceding changes.
- Produces: isolated desktop/Store validation roots and a locally launched desktop test build.

- [ ] Run isolated desktop and Store builds.
- [ ] Run `verify.ps1` with repeated captures and confirm native-only, latency, resources, mosaic, clipboard, Save dialog, and new interaction gates pass.
- [ ] Run `git diff --check` and scan for temporary debug markers.
- [ ] Update `AI_HANDOFF.md` with behavior, root causes, tests, remaining manual checks, and the no-release state.
- [ ] Back up the current `bin`, replace it with the validated desktop output, and launch the background app for user testing.
