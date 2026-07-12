#pragma once

#include <windows.h>

namespace ui_backbuffer {

using DrawCallback = void (*)(HWND window, HDC target);

void Draw(HWND window, HDC target, DrawCallback callback);

}  // namespace ui_backbuffer
