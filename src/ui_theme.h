#pragma once

#include <windows.h>

namespace ui_theme {

struct Theme {
    bool dark;
    COLORREF window;
    COLORREF card;
    COLORREF cardBorder;
    COLORREF cardHover;
    COLORREF elevated;
    COLORREF control;
    COLORREF controlHover;
    COLORREF controlBorder;
    COLORREF controlBorderHover;
    COLORREF text;
    COLORREF mutedText;
    COLORREF titleText;
    COLORREF track;
    COLORREF trackHover;
    COLORREF primary;
    COLORREF primaryHover;
    COLORREF primaryBorderHover;
    COLORREF knob;
    COLORREF disabledText;
};

COLORREF Rgb(BYTE r, BYTE g, BYTE b);
Theme BuildTheme();
void ApplySystemMenuTheme(bool dark);

}  // namespace ui_theme
