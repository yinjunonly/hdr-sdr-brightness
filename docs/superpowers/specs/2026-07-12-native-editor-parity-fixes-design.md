# Native Editor Parity Fixes Design

## Problem

The first pure C++ editor build introduced three user-visible regressions relative to the managed editor:

- Toolbar icons were redrawn as approximate GDI+ geometry instead of using the former Segoe Fluent icon glyphs.
- A mosaic stroke displayed as a translucent paint stroke and repeatedly sampled already-modified pixels, producing a smeared result instead of stable pixel blocks.
- Region interaction repainted through a newly allocated full-screen compatible bitmap for every mouse event, producing a low-frame-rate feel at 3440x1440.

## Approved Behavior

- Preserve freehand mosaic brush interaction.
- During the drag, show the actual block mosaic under the brush path rather than a gray placeholder line.
- After mouse-up, use a stable image-anchored cell grid and sample from an immutable pre-operation image so overlapping brush stamps cannot smear prior results.
- Restore the previous icon language: use Segoe Fluent Icons with Segoe MDL2 Assets fallback for the glyph-backed actions and port the previous hand-drawn marker, mosaic, cancel, copy, and exposure icons exactly.
- Keep every existing toolbar action, order, hit target, color, size popover, shortcut, and output format unchanged.

## Rendering Architecture

Add a native top-down 32-bit DIB surface abstraction. Region and preview windows retain source/rendered/frame surfaces for the life of the window. A paint copies only the invalid rectangle into the frame surface, clips overlays to that rectangle, and blits only that rectangle to the window. Mouse movement invalidates the union of the old and new selection or stroke bounds instead of the complete monitor.

The mosaic renderer builds a temporary brush mask for the operation bounds, computes one averaged source color per fixed grid cell, and fills only masked pixels. Live preview draws the same grid colors clipped by a widened GDI+ brush path.

## Verification Gates

- A deterministic image-core test verifies that an interior mosaic cell is uniform and equals the average of its original source cell while pixels outside the stroke remain unchanged.
- Toolbar tests verify the exact Fluent glyph mapping used by the previous editor.
- A 3440x1440 synchronous drag benchmark must sustain at least 55 painted frames per second on the current validation machine.
- Existing physical-pixel edge, clipboard PNG/CF_DIB, annotation, undo/redo, Save dialog, latency, desktop, and Store tests must remain green.
- No package, commit, release, upload, or Store submission is part of this correction.
