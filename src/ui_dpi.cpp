#include "ui_dpi.h"

namespace ui_dpi {

namespace {

int g_dpiX = 96;
int g_dpiY = 96;

Dpi Normalize(int dpiX, int dpiY) {
    if (dpiX <= 0) dpiX = 96;
    if (dpiY <= 0) dpiY = dpiX;
    return {dpiX, dpiY};
}

Dpi DpiFromWindow(HWND hwnd) {
    Dpi dpi = {96, 96};

    typedef UINT(WINAPI* GetDpiForWindowFn)(HWND);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    GetDpiForWindowFn getDpiForWindow =
        user32 ? reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow")) : NULL;
    if (hwnd && getDpiForWindow) {
        UINT value = getDpiForWindow(hwnd);
        if (value != 0) {
            return {static_cast<int>(value), static_cast<int>(value)};
        }
    }

    HDC dc = GetDC(hwnd);
    if (dc) {
        dpi.x = GetDeviceCaps(dc, LOGPIXELSX);
        dpi.y = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(hwnd, dc);
    }
    return Normalize(dpi.x, dpi.y);
}

Dpi DpiForNewTopLevelWindow(HWND owner) {
    Dpi dpi = {96, 96};
    bool found = false;

    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        typedef HRESULT(WINAPI* GetDpiForMonitorFn)(HMONITOR, int, UINT*, UINT*);
        GetDpiForMonitorFn getDpiForMonitor =
            reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(shcore, "GetDpiForMonitor"));
        if (getDpiForMonitor) {
            HMONITOR monitor = owner ? MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST) : NULL;
            if (!monitor) {
                POINT pt = {};
                GetCursorPos(&pt);
                monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
            }

            UINT monitorDpiX = 0;
            UINT monitorDpiY = 0;
            if (monitor && SUCCEEDED(getDpiForMonitor(monitor, 0, &monitorDpiX, &monitorDpiY)) &&
                monitorDpiX != 0) {
                dpi.x = static_cast<int>(monitorDpiX);
                dpi.y = static_cast<int>(monitorDpiY ? monitorDpiY : monitorDpiX);
                found = true;
            }
        }
        FreeLibrary(shcore);
    }

    if (!found) {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        typedef UINT(WINAPI* GetDpiForSystemFn)();
        GetDpiForSystemFn getDpiForSystem =
            user32 ? reinterpret_cast<GetDpiForSystemFn>(GetProcAddress(user32, "GetDpiForSystem")) : NULL;
        if (getDpiForSystem) {
            UINT value = getDpiForSystem();
            if (value != 0) {
                dpi.x = static_cast<int>(value);
                dpi.y = static_cast<int>(value);
                found = true;
            }
        }
    }

    if (!found) {
        HDC dc = GetDC(NULL);
        if (dc) {
            dpi.x = GetDeviceCaps(dc, LOGPIXELSX);
            dpi.y = GetDeviceCaps(dc, LOGPIXELSY);
            ReleaseDC(NULL, dc);
        }
    }

    return Normalize(dpi.x, dpi.y);
}

}  // namespace

Dpi Current() {
    return {g_dpiX, g_dpiY};
}

bool Set(int dpiX, int dpiY) {
    Dpi dpi = Normalize(dpiX, dpiY);
    if (dpi.x == g_dpiX && dpi.y == g_dpiY) {
        return false;
    }
    g_dpiX = dpi.x;
    g_dpiY = dpi.y;
    return true;
}

bool RefreshForWindow(HWND hwnd) {
    Dpi dpi = DpiFromWindow(hwnd);
    return Set(dpi.x, dpi.y);
}

bool RefreshForNewTopLevelWindow(HWND owner) {
    Dpi dpi = DpiForNewTopLevelWindow(owner);
    return Set(dpi.x, dpi.y);
}

int Scale(int value) {
    return MulDiv(value, g_dpiX, 96);
}

int Unscale(int value) {
    return MulDiv(value, 96, g_dpiX);
}

int FontHeightForPointSize(int pointSize) {
    return -MulDiv(pointSize, g_dpiY, 72);
}

RECT Box(int x, int y, int width, int height) {
    RECT rect = {Scale(x), Scale(y), Scale(x + width), Scale(y + height)};
    return rect;
}

bool PtInBox(POINT pt, int x, int y, int width, int height) {
    RECT rect = Box(x, y, width, height);
    return PtInRect(&rect, pt) != FALSE;
}

}  // namespace ui_dpi
