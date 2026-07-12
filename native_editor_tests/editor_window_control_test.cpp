#include "../src/editor_window_control.h"

#include <windows.h>

#include <cstdio>

namespace {

int g_closed = 0;

LRESULT CALLBACK TestWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CLOSE) {
        ++g_closed;
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
bool Register(const wchar_t* className, HINSTANCE instance) {
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = TestWindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    return RegisterClassW(&windowClass) != 0;
}

}  // namespace

int main() {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!Register(editor_window_control::kRegionWindowClass, instance) ||
        !Register(editor_window_control::kPreviewWindowClass, instance)) {
        std::fprintf(stderr, "FAIL: could not register native editor test classes.\n");
        return 1;
    }

    HWND regionOne = CreateWindowExW(0, editor_window_control::kRegionWindowClass,
                                      L"region one", WS_POPUP,
                                      0, 0, 10, 10, nullptr, nullptr, instance, nullptr);
    HWND regionTwo = CreateWindowExW(0, editor_window_control::kRegionWindowClass,
                                      L"region two", WS_POPUP,
                                      0, 0, 10, 10, nullptr, nullptr, instance, nullptr);
    HWND preview = CreateWindowExW(0, editor_window_control::kPreviewWindowClass,
                                   L"preview", WS_POPUP,
                                   0, 0, 10, 10, nullptr, nullptr, instance, nullptr);
    if (!regionOne || !regionTwo || !preview) {
        std::fprintf(stderr, "FAIL: could not create native editor test windows.\n");
        return 1;
    }

    int posted = editor_window_control::CloseAll();
    MSG message{};
    DWORD deadline = GetTickCount() + 1000;
    while (g_closed < 3 && static_cast<LONG>(GetTickCount() - deadline) < 0) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(1);
    }

    if (posted < 3 || g_closed != 3 ||
        IsWindow(regionOne) || IsWindow(regionTwo) || IsWindow(preview)) {
        std::fprintf(stderr,
                     "FAIL: CloseAll posted %d closes and destroyed %d of 3 windows.\n",
                     posted, g_closed);
        return 1;
    }

    std::puts("PASS: screenshot replacement closes every native editor window.");
    return 0;
}
