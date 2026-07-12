#include "ui_theme.h"

#include "registry_util.h"

namespace ui_theme {

COLORREF Rgb(BYTE r, BYTE g, BYTE b) {
    return RGB(r, g, b);
}

namespace {

bool ShouldUseDarkAppTheme() {
    DWORD lightTheme = 1;
    ReadDwordValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   L"AppsUseLightTheme", &lightTheme);
    return lightTheme == 0;
}

}  // namespace

void ApplySystemMenuTheme(bool dark) {
    HMODULE uxtheme = LoadLibraryW(L"uxtheme.dll");
    if (!uxtheme) return;

    typedef int(WINAPI* SetPreferredAppModeFn)(int);
    typedef void(WINAPI* FlushMenuThemesFn)();
    SetPreferredAppModeFn setPreferredAppMode =
        reinterpret_cast<SetPreferredAppModeFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
    FlushMenuThemesFn flushMenuThemes =
        reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));

    if (setPreferredAppMode) {
        setPreferredAppMode(dark ? 2 : 3);  // AllowDark / ForceLight.
    }
    if (flushMenuThemes) {
        flushMenuThemes();
    }

    FreeLibrary(uxtheme);
}

Theme BuildTheme() {
    Theme theme = {};
    theme.dark = ShouldUseDarkAppTheme();
    theme.primary = Rgb(0, 103, 192);
    theme.primaryHover = Rgb(0, 113, 212);
    theme.primaryBorderHover = Rgb(64, 178, 255);

    if (theme.dark) {
        theme.window = Rgb(15, 17, 18);
        theme.card = Rgb(31, 35, 36);
        theme.cardBorder = Rgb(31, 35, 36);
        theme.cardHover = Rgb(38, 42, 44);
        theme.elevated = Rgb(40, 45, 48);
        theme.control = Rgb(40, 45, 48);
        theme.controlHover = Rgb(49, 54, 57);
        theme.controlBorder = Rgb(40, 45, 48);
        theme.controlBorderHover = Rgb(62, 68, 71);
        theme.text = Rgb(241, 241, 241);
        theme.mutedText = Rgb(199, 202, 204);
        theme.titleText = Rgb(255, 255, 255);
        theme.track = Rgb(78, 84, 87);
        theme.trackHover = Rgb(94, 101, 105);
        theme.knob = Rgb(255, 255, 255);
        theme.disabledText = Rgb(142, 147, 150);
    } else {
        theme.window = Rgb(243, 243, 243);
        theme.card = Rgb(255, 255, 255);
        theme.cardBorder = Rgb(229, 229, 229);
        theme.cardHover = Rgb(247, 247, 247);
        theme.elevated = Rgb(249, 249, 249);
        theme.control = Rgb(251, 251, 251);
        theme.controlHover = Rgb(245, 245, 245);
        theme.controlBorder = Rgb(218, 218, 218);
        theme.controlBorderHover = Rgb(176, 176, 176);
        theme.text = Rgb(32, 32, 32);
        theme.mutedText = Rgb(96, 96, 96);
        theme.titleText = Rgb(24, 24, 24);
        theme.track = Rgb(210, 210, 210);
        theme.trackHover = Rgb(198, 198, 198);
        theme.knob = Rgb(255, 255, 255);
        theme.disabledText = Rgb(146, 146, 146);
    }
    return theme;
}

}  // namespace ui_theme
