#include "ui_dpi.h"

#include <windows.h>

#include <iostream>

int main() {
    ui_dpi::EnablePerMonitorV2();

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) {
        std::cerr << "FAIL: user32.dll is unavailable." << std::endl;
        return 1;
    }

    typedef HANDLE(WINAPI* GetThreadDpiAwarenessContextFn)();
    typedef BOOL(WINAPI* AreDpiAwarenessContextsEqualFn)(HANDLE, HANDLE);
    GetThreadDpiAwarenessContextFn getThreadContext =
        reinterpret_cast<GetThreadDpiAwarenessContextFn>(
            GetProcAddress(user32, "GetThreadDpiAwarenessContext"));
    AreDpiAwarenessContextsEqualFn areContextsEqual =
        reinterpret_cast<AreDpiAwarenessContextsEqualFn>(
            GetProcAddress(user32, "AreDpiAwarenessContextsEqual"));
    if (!getThreadContext || !areContextsEqual) {
        std::cerr << "FAIL: Windows does not expose DPI awareness context inspection." << std::endl;
        return 1;
    }

    HANDLE current = getThreadContext();
    HANDLE perMonitorV2 = reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4));
    if (!areContextsEqual(current, perMonitorV2)) {
        std::cerr << "FAIL: process did not enter Per-Monitor V2 DPI awareness." << std::endl;
        return 1;
    }

    std::cout << "PASS: startup enables Per-Monitor V2 DPI awareness." << std::endl;
    return 0;
}
