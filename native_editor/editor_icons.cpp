#include "editor_icons.h"

#include <cmath>

namespace editor {

namespace {

void DrawFluentGlyph(Gdiplus::Graphics* graphics,
                     const RECT& bounds,
                     const wchar_t* glyph,
                     Gdiplus::Color color) {
    Gdiplus::FontFamily fluent(L"Segoe Fluent Icons");
    Gdiplus::FontFamily mdl2(L"Segoe MDL2 Assets");
    Gdiplus::FontFamily* family = fluent.IsAvailable() ? &fluent : &mdl2;
    Gdiplus::Font font(family, 18.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::SolidBrush brush(color);
    Gdiplus::RectF rect(static_cast<float>(bounds.left),
                        static_cast<float>(bounds.top),
                        static_cast<float>(bounds.right - bounds.left),
                        static_cast<float>(bounds.bottom - bounds.top));
    graphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    graphics->DrawString(glyph, -1, &font, rect, &format, &brush);
}

}  // namespace

const wchar_t* FluentGlyphForEditorIcon(EditorIcon icon) {
    switch (icon) {
    case EditorIcon::Ellipse: return L"\xEA3A";
    case EditorIcon::Pen: return L"\xED63";
    case EditorIcon::Color: return L"\xE790";
    case EditorIcon::Undo: return L"\xE7A7";
    case EditorIcon::Redo: return L"\xE7A6";
    case EditorIcon::Reset: return L"\xE72C";
    case EditorIcon::Save: return L"\xE74E";
    default: return L"";
    }
}

void DrawEditorIcon(Gdiplus::Graphics* graphics,
                    const RECT& bounds,
                    EditorIcon icon,
                    Gdiplus::Color color) {
    if (!graphics) return;
    const wchar_t* glyph = FluentGlyphForEditorIcon(icon);
    if (glyph[0] != L'\0') {
        DrawFluentGlyph(graphics, bounds, glyph, color);
        return;
    }
    graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(color, 2.1f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::SolidBrush brush(color);
    int left = bounds.left;
    int top = bounds.top;
    int right = bounds.right;
    int bottom = bounds.bottom;
    int width = right - left;
    int height = bottom - top;
    int cx = left + width / 2;
    int cy = top + height / 2;

    switch (icon) {
    case EditorIcon::Cancel:
        graphics->DrawLine(&pen, left + 5, top + 5, right - 5, bottom - 5);
        graphics->DrawLine(&pen, right - 5, top + 5, left + 5, bottom - 5);
        break;
    case EditorIcon::Marker:
        graphics->DrawLine(&pen, left + 4, top + 6, right - 4, top + 6);
        graphics->DrawLine(&pen, right - 4, top + 6, right - 4, bottom - 6);
        graphics->DrawLine(&pen, right - 4, bottom - 6, left + 4, bottom - 6);
        graphics->DrawLine(&pen, left + 4, bottom - 6, left + 4, top + 6);
        break;
    case EditorIcon::Mosaic: {
        Gdiplus::SolidBrush dimBrush(Gdiplus::Color(115, color.GetR(), color.GetG(), color.GetB()));
        const int cell = 5;
        const int gap = 2;
        int gridLeft = cx - (cell * 3 + gap * 2) / 2;
        int gridTop = cy - (cell * 3 + gap * 2) / 2;
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                Gdiplus::SolidBrush* cellBrush = ((column + row) & 1) == 0 ? &brush : &dimBrush;
                graphics->FillRectangle(cellBrush,
                                        gridLeft + column * (cell + gap),
                                        gridTop + row * (cell + gap),
                                        cell,
                                        cell);
            }
        }
        break;
    }
    case EditorIcon::Low:
    case EditorIcon::Balanced:
    case EditorIcon::High: {
        int level = icon == EditorIcon::Low ? 1 : icon == EditorIcon::Balanced ? 2 : 3;
        const int core = 8;
        graphics->FillEllipse(&brush, cx - core / 2, cy - core / 2, core, core);
        int rays = level == 1 ? 4 : level == 2 ? 6 : 8;
        int outer = level == 1 ? 11 : level == 2 ? 12 : 13;
        for (int ray = 0; ray < rays; ++ray) {
            double angle = 3.14159265358979323846 * 2.0 * ray / rays -
                3.14159265358979323846 / 2.0;
            graphics->DrawLine(&pen,
                cx + static_cast<int>(std::lround(std::cos(angle) * 9)),
                cy + static_cast<int>(std::lround(std::sin(angle) * 9)),
                cx + static_cast<int>(std::lround(std::cos(angle) * outer)),
                cy + static_cast<int>(std::lround(std::sin(angle) * outer)));
        }
        break;
    }
    case EditorIcon::Copy: {
        Gdiplus::Point points[] = {
            {left + 3, cy},
            {cx - 2, bottom - 5},
            {right - 3, top + 5}
        };
        graphics->DrawLines(&pen, points, 3);
        break;
    }
    case EditorIcon::Ellipse:
    case EditorIcon::Pen:
    case EditorIcon::Color:
    case EditorIcon::Undo:
    case EditorIcon::Redo:
    case EditorIcon::Reset:
    case EditorIcon::Save:
        break;
    }
}

}  // namespace editor
