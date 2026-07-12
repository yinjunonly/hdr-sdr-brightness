#include "ui_window.h"

#include <algorithm>

namespace ui_window {

void ApplyModernFrame(HWND hwnd, bool dark, COLORREF captionColor, COLORREF textColor) {
    HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
    if (!dwmapi) return;

    typedef HRESULT(WINAPI* DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
    DwmSetWindowAttributeFn setAttribute =
        reinterpret_cast<DwmSetWindowAttributeFn>(GetProcAddress(dwmapi, "DwmSetWindowAttribute"));
    if (setAttribute) {
        BOOL darkFrame = dark ? TRUE : FALSE;
        setAttribute(hwnd, 20, &darkFrame, sizeof(darkFrame));  // DWMWA_USE_IMMERSIVE_DARK_MODE.
        setAttribute(hwnd, 19, &darkFrame, sizeof(darkFrame));  // Older dark mode attribute.

        DWORD corner = 2;  // DWMWCP_ROUND.
        setAttribute(hwnd, 33, &corner, sizeof(corner));

        DWORD backdrop = 2;  // DWMSBT_MAINWINDOW (Mica) on Windows 11 22H2+.
        setAttribute(hwnd, 38, &backdrop, sizeof(backdrop));

        setAttribute(hwnd, 35, &captionColor, sizeof(captionColor));  // DWMWA_CAPTION_COLOR on Windows 11.
        setAttribute(hwnd, 36, &textColor, sizeof(textColor));        // DWMWA_TEXT_COLOR on Windows 11.
    }

    FreeLibrary(dwmapi);
}

void AddRoundedRectPath(Gdiplus::GraphicsPath* path, Gdiplus::REAL x, Gdiplus::REAL y,
                        Gdiplus::REAL w, Gdiplus::REAL h, Gdiplus::REAL radius) {
    radius = std::max<Gdiplus::REAL>(0.0f, std::min<Gdiplus::REAL>(radius, std::min(w, h) / 2.0f));
    Gdiplus::REAL d = radius * 2.0f;
    if (radius <= 0.0f) {
        path->AddRectangle(Gdiplus::RectF(x, y, w, h));
        return;
    }
    path->AddArc(x, y, d, d, 180.0f, 90.0f);
    path->AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path->AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path->AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path->CloseFigure();
}

}  // namespace ui_window
