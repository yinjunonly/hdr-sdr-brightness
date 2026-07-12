#pragma once

#include "editor_icons.h"
#include "image_document.h"

#include <windows.h>

#include <vector>

namespace editor {

enum class EditMode {
    None,
    Marker,
    Ellipse,
    Pen,
    Mosaic
};

enum class ToolbarAction {
    Cancel,
    ToolMarker,
    ToolEllipse,
    ToolPen,
    ToolMosaic,
    Color,
    Undo,
    Redo,
    Reset,
    PresetLow,
    PresetBalanced,
    PresetHigh,
    Save,
    Copy
};

struct ToolbarItem {
    ToolbarAction action = ToolbarAction::Cancel;
    EditorIcon icon = EditorIcon::Cancel;
    RECT rect{};
};

struct ToolbarLayout {
    RECT bounds{};
    std::vector<ToolbarItem> items;
};

struct ToolbarVisualState {
    EditMode mode = EditMode::None;
    AdjustmentPreset preset = AdjustmentPreset::Balanced;
    int annotationColorIndex = 0;
    int hoveredItem = -1;
    int pressedItem = -1;
};

enum class ToolbarOptionType {
    Size,
    Color
};

struct ToolbarOptionItem {
    ToolbarOptionType type = ToolbarOptionType::Size;
    RECT rect{};
    int value = 0;
    bool selected = false;
};

struct ToolbarOptions {
    bool visible = false;
    RECT bounds{};
    std::vector<ToolbarOptionItem> items;
    int hoveredItem = -1;
    int pressedItem = -1;
};

ToolbarLayout LayoutSelectionToolbar(const RECT& selection, int clientWidth, int clientHeight);
ToolbarLayout LayoutPreviewToolbar(int centerX, int top);
RECT LayoutToolbarOptions(const RECT& anchor,
                          const RECT& toolbar,
                          int width,
                          int height,
                          int clientWidth,
                          int clientHeight);
int HitTestToolbar(const ToolbarLayout& layout, POINT point);
ToolbarOptions LayoutSizeOptions(ToolbarAction action,
                                 const RECT& anchor,
                                 const RECT& toolbar,
                                 int selectedValue,
                                 int clientWidth,
                                 int clientHeight);
ToolbarOptions LayoutColorOptions(const RECT& anchor,
                                  const RECT& toolbar,
                                  int selectedColor,
                                  int clientWidth,
                                  int clientHeight);
int HitTestToolbarOptions(const ToolbarOptions& options, POINT point);
COLORREF AnnotationColorAt(int index);
int AnnotationColorCount();
void DrawEditorToolbar(Gdiplus::Graphics* graphics,
                       const ToolbarLayout& layout,
                       const ToolbarVisualState& state);
void DrawToolbarOptions(Gdiplus::Graphics* graphics, const ToolbarOptions& options);

}  // namespace editor
