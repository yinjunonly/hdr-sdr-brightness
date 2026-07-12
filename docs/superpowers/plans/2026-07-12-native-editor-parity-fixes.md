# Native Editor Parity Fixes Implementation Plan

> **For agentic workers:** Execute inline in the current dirty workspace. Use test-first slices and do not commit, package, or publish without explicit user approval.

**Goal:** Restore managed-editor icon fidelity, make mosaic brush output visibly pixelated, and remove low-frame-rate region dragging from the pure C++ editor.

**Architecture:** Keep the existing Win32 editor and introduce a reusable persistent DIB surface. Make mosaic rendering snapshot-based and grid-aligned, then share its visual rules with the live GDI+ preview.

**Tech Stack:** C++17, Win32, GDI+, top-down DIB sections, PowerShell UI integration tests, MinGW-w64.

## Global Constraints

- Preserve Windows 10 1809+, x64, physical-pixel coordinates, seven languages, and system-DLL-only runtime behavior.
- Keep the freehand mosaic brush.
- Do not touch or publish GitHub/Store releases.
- Update `AI_HANDOFF.md` after product code changes.

---

### Task 1: Establish Regression Signals

**Files:**
- Modify: `native_editor_tests/image_document_test.cpp`
- Modify: `native_editor_tests/toolbar_layout_test.cpp`
- Create: `native_editor_tests/region_drag_performance_test.ps1`

- [ ] Add a grid-cell mosaic assertion that fails against sequential center-sample stamping.
- [ ] Add exact previous-editor Fluent glyph mapping assertions.
- [ ] Record the current 3440x1440 synchronous selection-drag frame rate and enforce a 55 FPS gate.

### Task 2: Restore Toolbar Icon Fidelity

**Files:**
- Modify: `native_editor/editor_icons.h`
- Modify: `native_editor/editor_icons.cpp`

- [ ] Expose the Fluent glyph mapping for deterministic tests.
- [ ] Render glyph-backed actions with Segoe Fluent Icons and MDL2 fallback.
- [ ] Port the previous custom geometry for the remaining icons without changing hit targets.

### Task 3: Render True Mosaic Brush Pixels

**Files:**
- Modify: `native_editor/mosaic_renderer.h`
- Modify: `native_editor/mosaic_renderer.cpp`
- Modify: `native_editor/region_selection.cpp`
- Modify: `native_editor/preview_editor.cpp`

- [ ] Replace sequential re-sampling with an immutable snapshot, image-anchored grid, and brush mask.
- [ ] Draw the same block grid during a region mosaic drag.
- [ ] Preserve rectangle mosaic behavior in fullscreen preview while making its live preview pixelated.

### Task 4: Reuse Paint Surfaces And Invalidate Locally

**Files:**
- Create: `native_editor/dib_surface.h`
- Create: `native_editor/dib_surface.cpp`
- Modify: `native_editor/build.ps1`
- Modify: `native_editor/region_selection.cpp`
- Modify: `native_editor/preview_editor.cpp`

- [ ] Keep source, rendered, and frame DIB sections alive for the window lifetime.
- [ ] Clip paint work and final BitBlt to `PAINTSTRUCT.rcPaint`.
- [ ] Invalidate only changed selection, toolbar, options, and active-stroke bounds.
- [ ] Pass the 55 FPS drag gate without changing screenshot pixels.

### Task 5: Full Regression And Local Replacement

**Files:**
- Modify: `verify.ps1`
- Modify: `AI_HANDOFF.md`

- [ ] Build isolated desktop and Store roots and run the complete non-packaging verification suite.
- [ ] Run ten capture-to-visible timings and the 50-capture lifetime test.
- [ ] After validation, replace local `bin` only because the user explicitly requested a test build, restart it in the background, and verify no managed process is present.
