#include "app_hotkeys.h"

#include <cwchar>

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

namespace app_hotkeys {

UINT ModifierMask() {
    return MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN;
}

void Unregister(HWND hwnd, int screenshotId, int fullscreenId) {
    if (!hwnd) return;
    UnregisterHotKey(hwnd, screenshotId);
    UnregisterHotKey(hwnd, fullscreenId);
}

void Register(HWND hwnd, int screenshotId, Binding screenshot,
              int fullscreenId, Binding fullscreen) {
    if (!hwnd) return;
    Unregister(hwnd, screenshotId, fullscreenId);
    if (screenshot.vk != 0) {
        RegisterHotKey(hwnd, screenshotId, screenshot.modifiers | MOD_NOREPEAT, screenshot.vk);
    }
    if (fullscreen.vk != 0) {
        RegisterHotKey(hwnd, fullscreenId, fullscreen.modifiers | MOD_NOREPEAT, fullscreen.vk);
    }
}

std::wstring Format(UINT modifiers, UINT vk, const std::wstring& noneText) {
    if (vk == 0) return noneText;

    std::wstring result;
    if (modifiers & MOD_CONTROL) result += L"Ctrl + ";
    if (modifiers & MOD_SHIFT) result += L"Shift + ";
    if (modifiers & MOD_ALT) result += L"Alt + ";
    if (modifiers & MOD_WIN) result += L"Win + ";

    if (vk >= 'A' && vk <= 'Z') {
        result += static_cast<wchar_t>(vk);
    } else if (vk >= VK_F1 && vk <= VK_F12) {
        wchar_t fnum[8] = {};
        _snwprintf(fnum, 8, L"F%u", vk - VK_F1 + 1);
        result += fnum;
    } else {
        UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        wchar_t keyName[64] = {};
        if (GetKeyNameTextW(static_cast<LONG>(scanCode) << 16, keyName, 64) > 0) {
            result += keyName;
        } else {
            wchar_t hex[16] = {};
            _snwprintf(hex, 16, L"0x%02X", vk);
            result += hex;
        }
    }
    return result;
}

}  // namespace app_hotkeys
