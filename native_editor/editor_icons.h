#pragma once

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

namespace editor {

enum class EditorIcon {
    Cancel,
    Marker,
    Ellipse,
    Pen,
    Mosaic,
    Color,
    Undo,
    Redo,
    Reset,
    Low,
    Balanced,
    High,
    Save,
    Copy
};

const wchar_t* FluentGlyphForEditorIcon(EditorIcon icon);
void DrawEditorIcon(Gdiplus::Graphics* graphics,
                    const RECT& bounds,
                    EditorIcon icon,
                    Gdiplus::Color color);

}  // namespace editor
