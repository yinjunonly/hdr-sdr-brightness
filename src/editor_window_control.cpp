#include "editor_window_control.h"

#include <windows.h>

#include <cwchar>

namespace editor_window_control {

const wchar_t kRegionWindowClass[] = L"HdrSdrNativeEditorRegionWindow";
const wchar_t kPreviewWindowClass[] = L"HdrSdrNativeEditorPreviewWindow";

namespace {

BOOL CALLBACK CloseEditorWindow(HWND hwnd, LPARAM value) {
    wchar_t className[128] = {};
    if (GetClassNameW(hwnd, className,
                      static_cast<int>(sizeof(className) / sizeof(className[0]))) == 0) {
        return TRUE;
    }
    if (std::wcscmp(className, kRegionWindowClass) != 0 &&
        std::wcscmp(className, kPreviewWindowClass) != 0) {
        return TRUE;
    }
    if (PostMessageW(hwnd, WM_CLOSE, 0, 0)) {
        ++*reinterpret_cast<int*>(value);
    }
    return TRUE;
}

}  // namespace

int CloseAll() {
    int posted = 0;
    EnumWindows(CloseEditorWindow, reinterpret_cast<LPARAM>(&posted));
    return posted;
}

}  // namespace editor_window_control
