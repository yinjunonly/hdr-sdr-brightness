#pragma once

#include <windows.h>

#include <string>

namespace app_hotkeys {

struct Binding {
    UINT modifiers;
    UINT vk;
};

UINT ModifierMask();
void Register(HWND hwnd, int screenshotId, Binding screenshot,
              int fullscreenId, Binding fullscreen);
void Unregister(HWND hwnd, int screenshotId, int fullscreenId);
std::wstring Format(UINT modifiers, UINT vk, const std::wstring& noneText);

}  // namespace app_hotkeys
