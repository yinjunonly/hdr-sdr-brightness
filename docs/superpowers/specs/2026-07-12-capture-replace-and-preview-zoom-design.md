# Capture Replacement And Preview Zoom Design

## Goal

Make interactive HDR screenshots strictly single-window while treating a repeated screenshot hotkey as a request to discard the current selection and capture the desktop again. Make fullscreen preview practical for detailed annotation by adding maximize/restore and cursor-centered wheel zoom.

## Confirmed Interaction

- A repeated region screenshot hotkey discards the visible selection/editor and its unsaved annotations.
- The newest request wins. Intermediate requests from rapid key presses must not each open a window.
- A capture already in progress may finish, but a stale result must never launch an editor or replace the newest image.
- At most one region or fullscreen editor window may remain visible.
- Fullscreen preview exposes a maximize/restore button in the custom title bar.
- The mouse wheel zooms around the pointer from fit-to-window (`1x`) through `8x`.
- At zoom levels above fit, middle-button dragging pans the image.
- Zooming and panning must preserve physical-image coordinate mapping for every annotation and clipboard/save output.

## Architecture

### Latest-request capture coordinator

Add a small UI-thread-owned state machine. The first request starts capture. Further requests received while capture is active only update the latest generation and mark one replacement pending. When the active capture completes, its stale result is discarded and exactly one capture for the latest generation starts. Only a completion whose generation is still current may launch the region editor.

The main window owns editor replacement. On every new interactive request it sends `WM_CLOSE` to all known native region/preview editor windows. It repeats this cleanup immediately before launching the newest editor, covering the process-creation race without terminating unrelated processes.

### Preview viewport

Move fit, zoom, pan, and image/client coordinate conversion into a testable `PreviewViewport` model. It stores image dimensions, available client bounds, zoom, and image-space center. Wheel zoom solves the new center so the image pixel under the pointer remains stationary. Clamping centers small images and prevents blank space around zoomed images.

The preview window continues rendering the full-resolution image through the existing persistent DIB surface. Zoom changes only the destination rectangle; edit operations remain stored in physical image pixels.

### Custom title control

Add a maximize/restore rectangle immediately left of Close. Clicking it calls `ShowWindow(SW_MAXIMIZE)` or `ShowWindow(SW_RESTORE)`. `WM_GETMINMAXINFO` uses the nearest monitor work area so maximization does not cover the taskbar. The icon switches between maximize and restore states.

## Error And Race Handling

- Capture failure is shown only when the failed generation is still the newest request.
- Failure from a stale generation is silent; the pending newest request starts immediately.
- Failure to start a worker is fed through the same completion state machine.
- Closing an old editor is best-effort and idempotent; the launch path closes known editor classes a second time before showing the replacement.
- Wheel and pan input are ignored during an active annotation stroke.

## Validation

- Unit-test first/pending/latest completion decisions and failure coalescing.
- Unit-test fit bounds, cursor anchoring, zoom limits, pan direction, clamping, and image/client round trips.
- Integration-test custom maximize/restore against a real native preview window.
- Re-run the native editor suite, desktop/Store builds, native-only package gates, physical-pixel region test, mosaic test, preview annotation test, latency test, and resource-lifetime test.
- Replace local `bin` only after isolated validation passes; the user then checks rapid hotkey replacement and real-wheel annotation.
