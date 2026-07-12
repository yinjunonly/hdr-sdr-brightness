#pragma once

#include <windows.h>
#include <gdiplus.h>

namespace ui_window {

void ApplyModernFrame(HWND hwnd, bool dark, COLORREF captionColor, COLORREF textColor);
void AddRoundedRectPath(Gdiplus::GraphicsPath* path, Gdiplus::REAL x, Gdiplus::REAL y,
                        Gdiplus::REAL w, Gdiplus::REAL h, Gdiplus::REAL radius);

}  // namespace ui_window
