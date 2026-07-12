#include "editor_toolbar.h"

#include <algorithm>
#include <cwchar>

namespace editor {

namespace {

const COLORREF kAnnotationColors[] = {
    RGB(255, 59, 48),
    RGB(255, 204, 0),
    RGB(52, 199, 89),
    RGB(64, 156, 255),
    RGB(191, 90, 242),
    RGB(255, 255, 255)
};

struct ActionIcon {
    ToolbarAction action;
    EditorIcon icon;
};

const ActionIcon kActions[] = {
    {ToolbarAction::Cancel, EditorIcon::Cancel},
    {ToolbarAction::ToolMarker, EditorIcon::Marker},
    {ToolbarAction::ToolEllipse, EditorIcon::Ellipse},
    {ToolbarAction::ToolPen, EditorIcon::Pen},
    {ToolbarAction::ToolMosaic, EditorIcon::Mosaic},
    {ToolbarAction::Color, EditorIcon::Color},
    {ToolbarAction::Undo, EditorIcon::Undo},
    {ToolbarAction::Redo, EditorIcon::Redo},
    {ToolbarAction::Reset, EditorIcon::Reset},
    {ToolbarAction::PresetLow, EditorIcon::Low},
    {ToolbarAction::PresetBalanced, EditorIcon::Balanced},
    {ToolbarAction::PresetHigh, EditorIcon::High},
    {ToolbarAction::Save, EditorIcon::Save},
    {ToolbarAction::Copy, EditorIcon::Copy}
};

bool PointInRect(const RECT& rect, POINT point) {
    return point.x >= rect.left && point.x < rect.right &&
        point.y >= rect.top && point.y < rect.bottom;
}

void AddRoundedRect(Gdiplus::GraphicsPath* path, const RECT& rect, float radius) {
    if (!path) return;
    float x = static_cast<float>(rect.left);
    float y = static_cast<float>(rect.top);
    float width = static_cast<float>(rect.right - rect.left);
    float height = static_cast<float>(rect.bottom - rect.top);
    float diameter = std::min(radius * 2.0f, std::min(width, height));
    if (diameter <= 1.0f) {
        path->AddRectangle(Gdiplus::RectF(x, y, width, height));
        return;
    }
    path->AddArc(x, y, diameter, diameter, 180, 90);
    path->AddArc(x + width - diameter, y, diameter, diameter, 270, 90);
    path->AddArc(x + width - diameter, y + height - diameter, diameter, diameter, 0, 90);
    path->AddArc(x, y + height - diameter, diameter, diameter, 90, 90);
    path->CloseFigure();
}

bool IsSelected(ToolbarAction action, const ToolbarVisualState& state) {
    switch (action) {
    case ToolbarAction::ToolMarker: return state.mode == EditMode::Marker;
    case ToolbarAction::ToolEllipse: return state.mode == EditMode::Ellipse;
    case ToolbarAction::ToolPen: return state.mode == EditMode::Pen;
    case ToolbarAction::ToolMosaic: return state.mode == EditMode::Mosaic;
    case ToolbarAction::PresetLow: return state.preset == AdjustmentPreset::Low;
    case ToolbarAction::PresetBalanced: return state.preset == AdjustmentPreset::Balanced;
    case ToolbarAction::PresetHigh: return state.preset == AdjustmentPreset::High;
    default: return false;
    }
}

Gdiplus::Color IconColor(ToolbarAction action, const ToolbarVisualState& state) {
    if (action == ToolbarAction::Cancel) return Gdiplus::Color(255, 255, 92, 92);
    if (action == ToolbarAction::Copy) return Gdiplus::Color(255, 68, 214, 111);
    if (action == ToolbarAction::Color) {
        COLORREF color = AnnotationColorAt(state.annotationColorIndex);
        return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
    }
    if (IsSelected(action, state)) return Gdiplus::Color(255, 64, 178, 255);
    return Gdiplus::Color(255, 225, 229, 232);
}

ToolbarLayout LayoutToolbarAt(int left, int top) {
    ToolbarLayout layout;
    layout.bounds = RECT{left, top, left + 820, top + 62};
    int x = left + 14;
    int y = top + 10;
    for (size_t index = 0; index < std::size(kActions); ++index) {
        if (index == 6 || index == 9 || index == 12) x += 13;
        layout.items.push_back(ToolbarItem{kActions[index].action,
                                           kActions[index].icon,
                                           RECT{x, y, x + 46, y + 42}});
        x += 54;
    }
    return layout;
}

}  // namespace

ToolbarLayout LayoutSelectionToolbar(const RECT& selection, int clientWidth, int clientHeight) {
    int left = selection.left + ((selection.right - selection.left) - 820) / 2;
    left = std::clamp(left, 12, std::max(12, clientWidth - 832));
    int top = selection.bottom + 10;
    if (top + 62 > clientHeight - 12) top = selection.top - 72;
    top = std::clamp(top, 12, std::max(12, clientHeight - 74));
    return LayoutToolbarAt(left, top);
}

ToolbarLayout LayoutPreviewToolbar(int centerX, int top) {
    return LayoutToolbarAt(centerX - 410, top);
}

ToolbarOptions BuildOptions(ToolbarOptionType type,
                            const std::vector<int>& values,
                            const RECT& anchor,
                            const RECT& toolbar,
                            int selectedValue,
                            int clientWidth,
                            int clientHeight) {
    ToolbarOptions options;
    if (values.empty()) return options;
    int width = 24 + static_cast<int>(values.size()) * 42 +
        std::max(0, static_cast<int>(values.size()) - 1) * 8;
    options.bounds = LayoutToolbarOptions(anchor, toolbar, width, 48,
                                          clientWidth, clientHeight);
    for (size_t index = 0; index < values.size(); ++index) {
        int left = options.bounds.left + 12 + static_cast<int>(index) * 50;
        options.items.push_back(ToolbarOptionItem{
            type,
            RECT{left, options.bounds.top + 7, left + 42, options.bounds.top + 41},
            values[index],
            values[index] == selectedValue
        });
    }
    options.visible = true;
    return options;
}

ToolbarOptions LayoutSizeOptions(ToolbarAction action,
                                 const RECT& anchor,
                                 const RECT& toolbar,
                                 int selectedValue,
                                 int clientWidth,
                                 int clientHeight) {
    std::vector<int> values;
    if (action == ToolbarAction::ToolMarker || action == ToolbarAction::ToolEllipse) {
        values = {2, 4, 6};
    } else if (action == ToolbarAction::ToolPen) {
        values = {3, 6, 10};
    } else if (action == ToolbarAction::ToolMosaic) {
        values = {16, 28, 42};
    }
    return BuildOptions(ToolbarOptionType::Size, values, anchor, toolbar,
                        selectedValue, clientWidth, clientHeight);
}

ToolbarOptions LayoutColorOptions(const RECT& anchor,
                                  const RECT& toolbar,
                                  int selectedColor,
                                  int clientWidth,
                                  int clientHeight) {
    std::vector<int> values;
    for (int index = 0; index < AnnotationColorCount(); ++index) values.push_back(index);
    return BuildOptions(ToolbarOptionType::Color, values, anchor, toolbar,
                        selectedColor, clientWidth, clientHeight);
}

int HitTestToolbarOptions(const ToolbarOptions& options, POINT point) {
    if (!options.visible || !PointInRect(options.bounds, point)) return -1;
    for (size_t index = 0; index < options.items.size(); ++index) {
        if (PointInRect(options.items[index].rect, point)) return static_cast<int>(index);
    }
    return -1;
}

RECT LayoutToolbarOptions(const RECT& anchor,
                          const RECT& toolbar,
                          int width,
                          int height,
                          int clientWidth,
                          int clientHeight) {
    int left = anchor.left + (anchor.right - anchor.left - width) / 2;
    left = std::clamp(left, 12, std::max(12, clientWidth - width - 12));
    int top = toolbar.top - height - 8;
    if (top < 12) top = toolbar.bottom + 8;
    top = std::clamp(top, 12, std::max(12, clientHeight - height - 12));
    return RECT{left, top, left + width, top + height};
}

int HitTestToolbar(const ToolbarLayout& layout, POINT point) {
    if (!PointInRect(layout.bounds, point)) return -1;
    for (size_t index = 0; index < layout.items.size(); ++index) {
        if (PointInRect(layout.items[index].rect, point)) return static_cast<int>(index);
    }
    return -1;
}

COLORREF AnnotationColorAt(int index) {
    int count = static_cast<int>(std::size(kAnnotationColors));
    index %= count;
    if (index < 0) index += count;
    return kAnnotationColors[index];
}

int AnnotationColorCount() {
    return static_cast<int>(std::size(kAnnotationColors));
}

void DrawEditorToolbar(Gdiplus::Graphics* graphics,
                       const ToolbarLayout& layout,
                       const ToolbarVisualState& state) {
    if (!graphics || layout.items.empty()) return;
    graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    RECT shadow = layout.bounds;
    OffsetRect(&shadow, 2, 4);
    shadow.right -= 3;
    shadow.bottom -= 3;
    Gdiplus::GraphicsPath shadowPath;
    AddRoundedRect(&shadowPath, shadow, 29.0f);
    Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(80, 0, 0, 0));
    graphics->FillPath(&shadowBrush, &shadowPath);

    RECT pill = layout.bounds;
    pill.right -= 2;
    pill.bottom -= 2;
    Gdiplus::GraphicsPath path;
    AddRoundedRect(&path, pill, 30.0f);
    Gdiplus::SolidBrush fill(Gdiplus::Color(215, 27, 30, 33));
    Gdiplus::Pen border(Gdiplus::Color(105, 255, 255, 255), 1.0f);
    graphics->FillPath(&fill, &path);
    graphics->DrawPath(&border, &path);

    for (size_t index = 0; index < layout.items.size(); ++index) {
        const ToolbarItem& item = layout.items[index];
        if (index == 6 || index == 9 || index == 12) {
            Gdiplus::Pen separator(Gdiplus::Color(70, 255, 255, 255), 1.0f);
            float x = static_cast<float>(item.rect.left - 11);
            graphics->DrawLine(&separator, x, static_cast<float>(layout.bounds.top + 19),
                               x, static_cast<float>(layout.bounds.bottom - 19));
        }

        bool selected = IsSelected(item.action, state);
        bool hot = static_cast<int>(index) == state.hoveredItem ||
            static_cast<int>(index) == state.pressedItem;
        if (selected || hot) {
            int size = hot ? 38 : 34;
            int cx = (item.rect.left + item.rect.right) / 2;
            int cy = (item.rect.top + item.rect.bottom) / 2;
            RECT hotRect{cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2};
            Gdiplus::GraphicsPath hotPath;
            AddRoundedRect(&hotPath, hotRect, size / 2.0f);
            Gdiplus::SolidBrush hotBrush(selected
                ? Gdiplus::Color(hot ? 58 : 38, 64, 178, 255)
                : Gdiplus::Color(44, 255, 255, 255));
            graphics->FillPath(&hotBrush, &hotPath);
        }
        RECT iconRect{item.rect.left + 10, item.rect.top + 8,
                      item.rect.left + 36, item.rect.top + 34};
        DrawEditorIcon(graphics, iconRect, item.icon, IconColor(item.action, state));
    }
}

void DrawToolbarOptions(Gdiplus::Graphics* graphics, const ToolbarOptions& options) {
    if (!graphics || !options.visible || options.items.empty()) return;
    graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    RECT shadow = options.bounds;
    OffsetRect(&shadow, 2, 4);
    shadow.right -= 3;
    shadow.bottom -= 3;
    Gdiplus::GraphicsPath shadowPath;
    AddRoundedRect(&shadowPath, shadow, 23.0f);
    Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(75, 0, 0, 0));
    graphics->FillPath(&shadowBrush, &shadowPath);

    RECT pill = options.bounds;
    pill.right -= 1;
    pill.bottom -= 1;
    Gdiplus::GraphicsPath path;
    AddRoundedRect(&path, pill, 24.0f);
    Gdiplus::SolidBrush fill(Gdiplus::Color(225, 27, 30, 33));
    Gdiplus::Pen border(Gdiplus::Color(105, 255, 255, 255), 1.0f);
    graphics->FillPath(&fill, &path);
    graphics->DrawPath(&border, &path);

    Gdiplus::FontFamily family(L"Segoe UI");
    Gdiplus::Font font(&family, 11.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    for (size_t index = 0; index < options.items.size(); ++index) {
        const ToolbarOptionItem& item = options.items[index];
        bool hot = static_cast<int>(index) == options.hoveredItem ||
            static_cast<int>(index) == options.pressedItem;
        if (hot || item.selected) {
            int size = item.selected ? 34 : 32;
            int cx = (item.rect.left + item.rect.right) / 2;
            int cy = (item.rect.top + item.rect.bottom) / 2;
            RECT hotRect{cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2};
            Gdiplus::GraphicsPath hotPath;
            AddRoundedRect(&hotPath, hotRect, size / 2.0f);
            Gdiplus::SolidBrush hotBrush(item.selected
                ? Gdiplus::Color(hot ? 62 : 42, 64, 178, 255)
                : Gdiplus::Color(48, 255, 255, 255));
            graphics->FillPath(&hotBrush, &hotPath);
        }
        if (item.type == ToolbarOptionType::Color) {
            COLORREF color = AnnotationColorAt(item.value);
            int diameter = item.selected ? 21 : 19;
            int x = (item.rect.left + item.rect.right - diameter) / 2;
            int y = (item.rect.top + item.rect.bottom - diameter) / 2;
            Gdiplus::SolidBrush swatch(Gdiplus::Color(255, GetRValue(color),
                                                      GetGValue(color), GetBValue(color)));
            graphics->FillEllipse(&swatch, x, y, diameter, diameter);
            Gdiplus::Pen ring(item.selected
                ? Gdiplus::Color(230, 225, 229, 232)
                : Gdiplus::Color(100, 255, 255, 255), item.selected ? 1.5f : 1.0f);
            graphics->DrawEllipse(&ring, x - (item.selected ? 4 : 0), y - (item.selected ? 4 : 0),
                                  diameter + (item.selected ? 8 : 0),
                                  diameter + (item.selected ? 8 : 0));
        } else {
            wchar_t value[12] = {};
            swprintf(value, std::size(value), L"%d", item.value);
            Gdiplus::SolidBrush text(item.selected
                ? Gdiplus::Color(255, 64, 178, 255)
                : Gdiplus::Color(255, 225, 229, 232));
            Gdiplus::RectF rect(static_cast<float>(item.rect.left),
                                static_cast<float>(item.rect.top),
                                static_cast<float>(item.rect.right - item.rect.left),
                                static_cast<float>(item.rect.bottom - item.rect.top));
            graphics->DrawString(value, -1, &font, rect, &format, &text);
        }
    }
}

}  // namespace editor
