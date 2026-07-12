#pragma once

#include <windows.h>

namespace ui_dpi {

struct Dpi {
    int x;
    int y;
};

Dpi Current();
bool Set(int dpiX, int dpiY);
bool RefreshForWindow(HWND hwnd);
bool RefreshForNewTopLevelWindow(HWND owner);
int Scale(int value);
int Unscale(int value);
int FontHeightForPointSize(int pointSize);
RECT Box(int x, int y, int width, int height);
bool PtInBox(POINT pt, int x, int y, int width, int height);

}  // namespace ui_dpi
