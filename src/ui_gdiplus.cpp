#include "ui_gdiplus.h"

#include <windows.h>
#include <gdiplus.h>

namespace ui_gdiplus {
namespace {

ULONG_PTR g_token = 0;

}  // namespace

bool EnsureStarted() {
    if (g_token) return true;

    Gdiplus::GdiplusStartupInput gdiplusInput;
    return Gdiplus::GdiplusStartup(&g_token, &gdiplusInput, NULL) == Gdiplus::Ok;
}

void Shutdown() {
    if (!g_token) return;
    Gdiplus::GdiplusShutdown(g_token);
    g_token = 0;
}

}  // namespace ui_gdiplus
