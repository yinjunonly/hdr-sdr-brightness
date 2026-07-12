#include "region_selection.h"

#include "editor_clipboard.h"
#include "dib_surface.h"
#include "editor_text.h"
#include "editor_toolbar.h"
#include "editor_tooltips.h"
#include "mosaic_renderer.h"
#include "wic_png.h"

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace editor {

namespace {

const wchar_t kRegionWindowClass[] = L"HdrSdrNativeEditorRegionWindow";

struct RegionState {
    RegionState(const EditorOptions& launchOptions, BgraImage sourceImage)
        : options(launchOptions), document(std::move(sourceImage)),
          previousForeground(GetForegroundWindow()) {
        surfacesReady = sourceSurface.Load(document.Source()) &&
            renderedSurface.Load(document.Source());
    }

    EditorOptions options;
    ImageDocument document;
    BgraImage rendered;
    DibSurface sourceSurface;
    DibSurface renderedSurface;
    DibSurface frameSurface;
    bool surfacesReady = false;
    HWND previousForeground = nullptr;
    RECT selection{};
    POINT dragStart{};
    POINT dragCurrent{};
    bool selecting = false;
    bool selectionReady = false;
    bool editing = false;
    POINT editStart{};
    POINT editCurrent{};
    std::vector<POINT> currentPoints;
    bool currentStrokeInside = false;
    ToolbarLayout toolbar;
    ToolbarVisualState toolbarState;
    ToolbarOptions toolbarOptions;
    int shapeStrokeWidth = 4;
    int penStrokeWidth = 6;
    int mosaicBrushSize = 28;
    bool accepted = false;
    HWND tooltip = nullptr;
};

RegionState* State(HWND hwnd) {
    return reinterpret_cast<RegionState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

const BgraImage& CurrentImage(const RegionState* state) {
    return state->rendered.IsValid() ? state->rendered : state->document.Source();
}

int MessageX(LPARAM value) {
    return static_cast<short>(LOWORD(value));
}

int MessageY(LPARAM value) {
    return static_cast<short>(HIWORD(value));
}

RECT Normalize(POINT first, POINT second) {
    return RECT{std::min(first.x, second.x), std::min(first.y, second.y),
                std::max(first.x, second.x), std::max(first.y, second.y)};
}

bool Contains(const RECT& rect, POINT point) {
    return point.x >= rect.left && point.x < rect.right &&
        point.y >= rect.top && point.y < rect.bottom;
}

RECT ClientBounds(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);
    return client;
}

RECT ClampedRect(HWND hwnd, RECT rect, int padding = 0) {
    if (padding != 0) InflateRect(&rect, padding, padding);
    RECT client = ClientBounds(hwnd);
    RECT clipped{};
    if (!IntersectRect(&clipped, &rect, &client)) return RECT{};
    return clipped;
}

void InvalidateArea(HWND hwnd, RECT rect, int padding = 0) {
    rect = ClampedRect(hwnd, rect, padding);
    if (rect.right > rect.left && rect.bottom > rect.top) {
        InvalidateRect(hwnd, &rect, FALSE);
    }
}

void InvalidateTransition(HWND hwnd, RECT previous, RECT current, int padding) {
    RECT combined{};
    if (!UnionRect(&combined, &previous, &current)) return;
    InvalidateArea(hwnd, combined, padding);
}

RECT PointBounds(POINT first, POINT second) {
    RECT bounds{std::min(first.x, second.x), std::min(first.y, second.y),
                std::max(first.x, second.x) + 1, std::max(first.y, second.y) + 1};
    return bounds;
}

void FillDim(Gdiplus::Graphics* graphics, const RECT& rect) {
    if (!graphics || rect.right <= rect.left || rect.bottom <= rect.top) return;
    Gdiplus::SolidBrush dim(Gdiplus::Color(145, 0, 0, 0));
    graphics->FillRectangle(&dim, static_cast<INT>(rect.left), static_cast<INT>(rect.top),
                            static_cast<INT>(rect.right - rect.left),
                            static_cast<INT>(rect.bottom - rect.top));
}

void DrawMosaicBrushPreview(Gdiplus::Graphics* graphics, const RegionState* state) {
    if (!graphics || !state || state->currentPoints.empty()) return;
    int radius = std::max(4, state->mosaicBrushSize / 2);
    Gdiplus::GraphicsPath path;
    POINT previous{};
    bool hasPrevious = false;
    bool hasLine = false;
    std::vector<POINT> isolated;
    for (POINT point : state->currentPoints) {
        if (IsPathBreak(point)) {
            hasPrevious = false;
            path.StartFigure();
            continue;
        }
        if (hasPrevious) {
            path.AddLine(static_cast<INT>(previous.x), static_cast<INT>(previous.y),
                         static_cast<INT>(point.x), static_cast<INT>(point.y));
            hasLine = true;
        } else {
            isolated.push_back(point);
        }
        previous = point;
        hasPrevious = true;
    }
    Gdiplus::Pen brushPen(Gdiplus::Color(255, 255, 255, 255),
                          static_cast<Gdiplus::REAL>(state->mosaicBrushSize));
    brushPen.SetStartCap(Gdiplus::LineCapRound);
    brushPen.SetEndCap(Gdiplus::LineCapRound);
    brushPen.SetLineJoin(Gdiplus::LineJoinRound);
    if (hasLine) path.Widen(&brushPen);
    for (POINT point : isolated) {
        path.AddEllipse(point.x - radius, point.y - radius, radius * 2, radius * 2);
    }

    Gdiplus::RectF pathBounds;
    path.GetBounds(&pathBounds);
    RECT bounds{static_cast<LONG>(std::floor(pathBounds.X)),
                static_cast<LONG>(std::floor(pathBounds.Y)),
                static_cast<LONG>(std::ceil(pathBounds.GetRight())),
                static_cast<LONG>(std::ceil(pathBounds.GetBottom()))};
    RECT clipped{};
    if (!IntersectRect(&clipped, &bounds, &state->selection)) return;

    int saved = graphics->Save();
    graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);
    graphics->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    graphics->SetClip(Gdiplus::Rect(state->selection.left, state->selection.top,
                                    state->selection.right - state->selection.left,
                                    state->selection.bottom - state->selection.top),
                      Gdiplus::CombineModeIntersect);
    graphics->SetClip(&path, Gdiplus::CombineModeIntersect);
    const BgraImage& image = CurrentImage(state);
    int cell = MosaicBrushCellSize(state->mosaicBrushSize);
    int firstX = static_cast<int>(std::floor(clipped.left / static_cast<double>(cell))) * cell;
    int firstY = static_cast<int>(std::floor(clipped.top / static_cast<double>(cell))) * cell;
    for (int y = firstY; y < clipped.bottom; y += cell) {
        for (int x = firstX; x < clipped.right; x += cell) {
            RECT grid{x, y, x + cell, y + cell};
            COLORREF value = MosaicCellAverage(image, grid);
            Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(value),
                                                     GetGValue(value), GetBValue(value)));
            graphics->FillRectangle(&brush, x, y, cell, cell);
        }
    }
    graphics->Restore(saved);
}

void DrawDimOutside(Gdiplus::Graphics* graphics, const RECT& client, const RECT* selection) {
    if (!selection) {
        FillDim(graphics, client);
        return;
    }
    FillDim(graphics, RECT{client.left, client.top, client.right, selection->top});
    FillDim(graphics, RECT{client.left, selection->bottom, client.right, client.bottom});
    FillDim(graphics, RECT{client.left, selection->top, selection->left, selection->bottom});
    FillDim(graphics, RECT{selection->right, selection->top, client.right, selection->bottom});
}

void DrawSelectionChrome(Gdiplus::Graphics* graphics, const RECT& selection) {
    if (!graphics || selection.right <= selection.left || selection.bottom <= selection.top) return;
    graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen outer(Gdiplus::Color(255, 245, 252, 255), 2.0f);
    Gdiplus::Pen accent(Gdiplus::Color(255, 0, 153, 255), 1.0f);
    graphics->DrawRectangle(&outer, static_cast<INT>(selection.left),
                            static_cast<INT>(selection.top),
                            static_cast<INT>(selection.right - selection.left),
                            static_cast<INT>(selection.bottom - selection.top));
    graphics->DrawRectangle(&accent, static_cast<INT>(selection.left + 2),
                            static_cast<INT>(selection.top + 2),
                            static_cast<INT>(std::max(0L, selection.right - selection.left - 4)),
                            static_cast<INT>(std::max(0L, selection.bottom - selection.top - 4)));
    Gdiplus::SolidBrush handle(Gdiplus::Color(255, 255, 255, 255));
    POINT points[] = {
        {selection.left, selection.top}, {selection.right, selection.top},
        {selection.left, selection.bottom}, {selection.right, selection.bottom}
    };
    for (POINT point : points) {
        graphics->FillRectangle(&handle, point.x - 3, point.y - 3, 7, 7);
    }
}

void DrawCurrentEdit(Gdiplus::Graphics* graphics, const RegionState* state) {
    if (!graphics || !state || !state->editing) return;
    if (state->toolbarState.mode == EditMode::Mosaic) {
        DrawMosaicBrushPreview(graphics, state);
        return;
    }
    COLORREF value = AnnotationColorAt(state->toolbarState.annotationColorIndex);
    Gdiplus::Color color(255, GetRValue(value), GetGValue(value), GetBValue(value));
    int width = state->toolbarState.mode == EditMode::Pen
        ? state->penStrokeWidth
        : state->shapeStrokeWidth;
    Gdiplus::Pen pen(color, static_cast<Gdiplus::REAL>(std::max(1, width)));
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    if (state->toolbarState.mode == EditMode::Pen && state->currentPoints.size() > 1) {
        std::vector<Gdiplus::Point> segment;
        auto flush = [&]() {
            if (segment.size() > 1) {
                graphics->DrawLines(&pen, segment.data(), static_cast<INT>(segment.size()));
            }
            segment.clear();
        };
        for (POINT point : state->currentPoints) {
            if (IsPathBreak(point)) flush();
            else segment.emplace_back(point.x, point.y);
        }
        flush();
        return;
    }
    RECT rect = Normalize(state->editStart, state->editCurrent);
    if (state->toolbarState.mode == EditMode::Ellipse) {
        graphics->DrawEllipse(&pen, static_cast<INT>(rect.left), static_cast<INT>(rect.top),
                              static_cast<INT>(rect.right - rect.left),
                              static_cast<INT>(rect.bottom - rect.top));
    } else if (state->toolbarState.mode == EditMode::Marker) {
        graphics->DrawRectangle(&pen, static_cast<INT>(rect.left), static_cast<INT>(rect.top),
                                static_cast<INT>(rect.right - rect.left),
                                static_cast<INT>(rect.bottom - rect.top));
    }
}

void PaintRegion(HWND hwnd, HDC destinationDc, RegionState* state, RECT dirty) {
    RECT client = ClientBounds(hwnd);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (!state->surfacesReady) {
        state->surfacesReady = state->sourceSurface.Load(state->document.Source()) &&
            state->renderedSurface.Load(CurrentImage(state));
    }
    if (!state->surfacesReady || !state->frameSurface.Ensure(width, height)) return;
    if (!IntersectRect(&dirty, &dirty, &client)) return;
    HDC bufferDc = state->frameSurface.Dc();
    BitBlt(bufferDc, dirty.left, dirty.top,
           dirty.right - dirty.left, dirty.bottom - dirty.top,
           state->sourceSurface.Dc(), dirty.left, dirty.top, SRCCOPY);

    RECT active = state->selecting ? Normalize(state->dragStart, state->dragCurrent) : state->selection;
    bool hasSelection = state->selecting || state->selectionReady;
    if (hasSelection && state->selectionReady) {
        RECT selectedDirty{};
        if (IntersectRect(&selectedDirty, &dirty, &active)) {
            BitBlt(bufferDc, selectedDirty.left, selectedDirty.top,
                   selectedDirty.right - selectedDirty.left,
                   selectedDirty.bottom - selectedDirty.top,
                   state->renderedSurface.Dc(), selectedDirty.left, selectedDirty.top, SRCCOPY);
        }
    }

    Gdiplus::Graphics graphics(bufferDc);
    graphics.SetClip(Gdiplus::Rect(dirty.left, dirty.top,
                                   dirty.right - dirty.left,
                                   dirty.bottom - dirty.top));
    DrawDimOutside(&graphics, client, hasSelection ? &active : nullptr);
    if (hasSelection) DrawSelectionChrome(&graphics, active);
    if (state->selectionReady) {
        int saved = graphics.Save();
        graphics.SetClip(Gdiplus::Rect(active.left, active.top,
                                       active.right - active.left, active.bottom - active.top));
        DrawCurrentEdit(&graphics, state);
        graphics.Restore(saved);
        DrawEditorToolbar(&graphics, state->toolbar, state->toolbarState);
        DrawToolbarOptions(&graphics, state->toolbarOptions);
    } else {
        Gdiplus::FontFamily family(GetEditorFontName(state->options.language));
        Gdiplus::Font font(&family, 15.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        const wchar_t* hint = GetEditorText(state->options.language, EditorTextId::SelectHint);
        Gdiplus::RectF textBounds;
        graphics.MeasureString(hint, -1, &font, Gdiplus::PointF(0, 0), &textBounds);
        Gdiplus::RectF background(16.0f, 16.0f, textBounds.Width + 24.0f, 34.0f);
        Gdiplus::SolidBrush labelBackground(Gdiplus::Color(205, 18, 22, 26));
        graphics.FillRectangle(&labelBackground, background);
        Gdiplus::SolidBrush labelText(Gdiplus::Color(255, 235, 239, 242));
        graphics.DrawString(hint, -1, &font, Gdiplus::PointF(28.0f, 24.0f), &labelText);
    }
    graphics.Flush(Gdiplus::FlushIntentionSync);
    BitBlt(destinationDc, dirty.left, dirty.top,
           dirty.right - dirty.left, dirty.bottom - dirty.top,
           bufferDc, dirty.left, dirty.top, SRCCOPY);
}

void Rebuild(RegionState* state) {
    if (!state) return;
    state->rendered = state->document.Render();
    state->surfacesReady = state->sourceSurface.Load(state->document.Source()) &&
        state->renderedSurface.Load(CurrentImage(state));
    state->toolbarState.preset = state->document.GetAdjustmentPreset();
}

bool PromptSave(HWND owner, int language, const std::wstring& defaultPath, std::wstring* selectedPath) {
    wchar_t path[32768] = {};
    wcsncpy(path, defaultPath.c_str(), std::size(path) - 1);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrTitle = GetEditorText(language, EditorTextId::SaveDialogTitle);
    dialog.lpstrFilter = L"PNG image (*.png)\0*.png\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = static_cast<DWORD>(std::size(path));
    dialog.lpstrDefExt = L"png";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&dialog)) return false;
    if (selectedPath) *selectedPath = path;
    return true;
}

bool Commit(HWND hwnd, RegionState* state, bool save) {
    if (!state || !state->selectionReady) return false;
    BgraImage output;
    if (!CropImage(CurrentImage(state), state->selection, &output)) return false;
    std::wstring error;
    bool succeeded = false;
    if (save) {
        std::wstring path;
        if (!PromptSave(hwnd, state->options.language, state->options.outputPath, &path)) return false;
        succeeded = SavePng(path, output, &error);
        if (!succeeded) MessageBoxW(hwnd, error.c_str(), L"HDR SDR Screenshot", MB_OK | MB_ICONERROR);
    } else {
        succeeded = CopyImageToClipboard(hwnd, output, &error);
        if (!succeeded) MessageBoxW(hwnd, error.c_str(), L"HDR SDR Screenshot", MB_OK | MB_ICONERROR);
    }
    if (succeeded) {
        state->accepted = true;
        DestroyWindow(hwnd);
    }
    return succeeded;
}

const ToolbarItem* FindToolbarItem(const ToolbarLayout& toolbar, ToolbarAction action) {
    for (const ToolbarItem& item : toolbar.items) {
        if (item.action == action) return &item;
    }
    return nullptr;
}

int CurrentToolSize(const RegionState* state, EditMode mode) {
    if (mode == EditMode::Marker || mode == EditMode::Ellipse) return state->shapeStrokeWidth;
    if (mode == EditMode::Pen) return state->penStrokeWidth;
    if (mode == EditMode::Mosaic) return state->mosaicBrushSize;
    return 0;
}

void SetTool(HWND hwnd, RegionState* state, EditMode mode, ToolbarAction action) {
    if (!state) return;
    bool alreadySelected = state->toolbarState.mode == mode;
    state->toolbarState.mode = mode;
    state->toolbarOptions.visible = false;
    if (alreadySelected) {
        const ToolbarItem* anchor = FindToolbarItem(state->toolbar, action);
        RECT client = ClientBounds(hwnd);
        if (anchor) {
            state->toolbarOptions = LayoutSizeOptions(action, anchor->rect, state->toolbar.bounds,
                CurrentToolSize(state, mode), client.right, client.bottom);
        }
    }
}

void ApplyToolbarOption(RegionState* state, const ToolbarOptionItem& option) {
    if (option.type == ToolbarOptionType::Color) {
        state->toolbarState.annotationColorIndex = option.value;
    } else if (state->toolbarState.mode == EditMode::Marker ||
               state->toolbarState.mode == EditMode::Ellipse) {
        state->shapeStrokeWidth = option.value;
    } else if (state->toolbarState.mode == EditMode::Pen) {
        state->penStrokeWidth = option.value;
    } else if (state->toolbarState.mode == EditMode::Mosaic) {
        state->mosaicBrushSize = option.value;
    }
    state->toolbarOptions = ToolbarOptions{};
}

void ExecuteToolbarAction(HWND hwnd, RegionState* state, ToolbarAction action) {
    if (!state) return;
    switch (action) {
    case ToolbarAction::Cancel:
        DestroyWindow(hwnd);
        return;
    case ToolbarAction::ToolMarker: SetTool(hwnd, state, EditMode::Marker, action); break;
    case ToolbarAction::ToolEllipse: SetTool(hwnd, state, EditMode::Ellipse, action); break;
    case ToolbarAction::ToolPen: SetTool(hwnd, state, EditMode::Pen, action); break;
    case ToolbarAction::ToolMosaic: SetTool(hwnd, state, EditMode::Mosaic, action); break;
    case ToolbarAction::Color: {
        const ToolbarItem* anchor = FindToolbarItem(state->toolbar, action);
        RECT client = ClientBounds(hwnd);
        if (anchor) {
            state->toolbarOptions = LayoutColorOptions(anchor->rect, state->toolbar.bounds,
                state->toolbarState.annotationColorIndex, client.right, client.bottom);
        }
        break;
    }
    case ToolbarAction::Undo:
        state->document.Undo();
        Rebuild(state);
        break;
    case ToolbarAction::Redo:
        state->document.Redo();
        Rebuild(state);
        break;
    case ToolbarAction::Reset:
        {
        AdjustmentPreset preset = state->document.GetAdjustmentPreset();
        state->document.Reset();
        state->document.SetAdjustmentPreset(preset);
        Rebuild(state);
        break;
        }
    case ToolbarAction::PresetLow:
        state->document.SetAdjustmentPreset(AdjustmentPreset::Low);
        Rebuild(state);
        break;
    case ToolbarAction::PresetBalanced:
        state->document.SetAdjustmentPreset(AdjustmentPreset::Balanced);
        Rebuild(state);
        break;
    case ToolbarAction::PresetHigh:
        state->document.SetAdjustmentPreset(AdjustmentPreset::High);
        Rebuild(state);
        break;
    case ToolbarAction::Save:
        Commit(hwnd, state, true);
        return;
    case ToolbarAction::Copy:
        Commit(hwnd, state, false);
        return;
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

void FinishEdit(HWND hwnd, RegionState* state) {
    if (!state || !state->editing) return;
    state->editing = false;
    state->currentStrokeInside = false;
    ReleaseCapture();
    EditOperation operation;
    operation.color = AnnotationColorAt(state->toolbarState.annotationColorIndex);
    if (state->toolbarState.mode == EditMode::Pen || state->toolbarState.mode == EditMode::Mosaic) {
        size_t pointCount = 0;
        for (POINT point : state->currentPoints) {
            if (!IsPathBreak(point)) ++pointCount;
        }
        if (pointCount < 2) {
            state->currentPoints.clear();
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        operation.type = state->toolbarState.mode == EditMode::Pen
            ? EditOperationType::Pen
            : EditOperationType::Mosaic;
        operation.points = state->currentPoints;
        operation.strokeWidth = state->toolbarState.mode == EditMode::Pen
            ? state->penStrokeWidth
            : state->mosaicBrushSize;
    } else {
        operation.rect = Normalize(state->editStart, state->editCurrent);
        if (operation.rect.right - operation.rect.left < 4 ||
            operation.rect.bottom - operation.rect.top < 4) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        operation.type = state->toolbarState.mode == EditMode::Ellipse
            ? EditOperationType::Ellipse
            : EditOperationType::Marker;
        operation.strokeWidth = state->shapeStrokeWidth;
    }
    state->document.AddOperation(operation);
    state->currentPoints.clear();
    Rebuild(state);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void RestoreForeground(RegionState* state) {
    if (!state || !state->previousForeground || !IsWindow(state->previousForeground)) return;
    SetForegroundWindow(state->previousForeground);
}

LRESULT CALLBACK RegionWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    RegionState* state = State(hwnd);
    switch (message) {
    case WM_NCCREATE: {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    case WM_CREATE:
        if (state) state->tooltip = CreateToolbarTooltip(hwnd);
        return 0;
    case WM_SETCURSOR:
        if (state && state->selectionReady && state->toolbarState.hoveredItem >= 0) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
        } else {
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        }
        return TRUE;
    case WM_LBUTTONDOWN: {
        if (!state) return 0;
        POINT point{MessageX(lParam), MessageY(lParam)};
        int optionHit = state->selectionReady ? HitTestToolbarOptions(state->toolbarOptions, point) : -1;
        if (optionHit >= 0) {
            state->toolbarOptions.pressedItem = optionHit;
            SetCapture(hwnd);
            InvalidateArea(hwnd, state->toolbarOptions.bounds, 4);
            return 0;
        }
        if (state->toolbarOptions.visible) {
            state->toolbarOptions = ToolbarOptions{};
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        int toolbarHit = state->selectionReady ? HitTestToolbar(state->toolbar, point) : -1;
        if (toolbarHit >= 0) {
            state->toolbarState.pressedItem = toolbarHit;
            SetCapture(hwnd);
            InvalidateArea(hwnd, state->toolbar.bounds, 4);
            return 0;
        }
        if (state->selectionReady && state->toolbarState.mode != EditMode::None &&
            Contains(state->selection, point)) {
            state->editing = true;
            state->editStart = point;
            state->editCurrent = point;
            state->currentPoints.clear();
            if (state->toolbarState.mode == EditMode::Pen || state->toolbarState.mode == EditMode::Mosaic) {
                state->currentPoints.push_back(point);
                state->currentStrokeInside = true;
                int padding = state->toolbarState.mode == EditMode::Mosaic
                    ? state->mosaicBrushSize / 2 + 4
                    : state->penStrokeWidth + 4;
                InvalidateArea(hwnd, RECT{point.x, point.y, point.x + 1, point.y + 1}, padding);
            }
            SetCapture(hwnd);
            return 0;
        }
        state->selecting = true;
        state->selectionReady = false;
        state->dragStart = point;
        state->dragCurrent = point;
        AdjustmentPreset preset = state->document.GetAdjustmentPreset();
        state->document.Reset();
        state->document.SetAdjustmentPreset(preset);
        state->rendered = preset == AdjustmentPreset::Balanced
            ? BgraImage{}
            : state->document.Render();
        state->toolbarState.preset = preset;
        SetCapture(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!state) return 0;
        POINT point{MessageX(lParam), MessageY(lParam)};
        int optionHover = state->selectionReady ? HitTestToolbarOptions(state->toolbarOptions, point) : -1;
        if (optionHover != state->toolbarOptions.hoveredItem) {
            state->toolbarOptions.hoveredItem = optionHover;
            InvalidateArea(hwnd, state->toolbarOptions.bounds, 4);
        }
        int hover = optionHover >= 0 ? -1 :
            (state->selectionReady ? HitTestToolbar(state->toolbar, point) : -1);
        if (hover != state->toolbarState.hoveredItem) {
            state->toolbarState.hoveredItem = hover;
            InvalidateArea(hwnd, state->toolbar.bounds, 4);
        }
        if (state->selecting) {
            RECT previous = Normalize(state->dragStart, state->dragCurrent);
            state->dragCurrent = point;
            RECT current = Normalize(state->dragStart, state->dragCurrent);
            InvalidateTransition(hwnd, previous, current, 6);
        } else if (state->editing) {
            POINT previousPoint = state->editCurrent;
            state->editCurrent = point;
            if (state->toolbarState.mode == EditMode::Pen || state->toolbarState.mode == EditMode::Mosaic) {
                bool inside = Contains(state->selection, point);
                bool appended = false;
                if (inside) {
                    if (!state->currentStrokeInside && !state->currentPoints.empty() &&
                        !IsPathBreak(state->currentPoints.back())) {
                        state->currentPoints.push_back(PathBreakPoint());
                    }
                    if (state->currentPoints.empty() || IsPathBreak(state->currentPoints.back())) {
                        state->currentPoints.push_back(point);
                        appended = true;
                    } else {
                        POINT previous = state->currentPoints.back();
                        int dx = point.x - previous.x;
                        int dy = point.y - previous.y;
                        int threshold = state->toolbarState.mode == EditMode::Mosaic
                            ? std::max(3, state->mosaicBrushSize / 5)
                            : 2;
                        if (dx * dx + dy * dy >= threshold * threshold) {
                            state->currentPoints.push_back(point);
                            appended = true;
                        }
                    }
                }
                state->currentStrokeInside = inside;
                if (appended) {
                    int padding = state->toolbarState.mode == EditMode::Mosaic
                        ? state->mosaicBrushSize / 2 + MosaicBrushCellSize(state->mosaicBrushSize) + 3
                        : state->penStrokeWidth + 3;
                    InvalidateArea(hwnd, PointBounds(previousPoint, point), padding);
                }
            } else {
                RECT previous = Normalize(state->editStart, previousPoint);
                RECT current = Normalize(state->editStart, point);
                InvalidateTransition(hwnd, previous, current, state->shapeStrokeWidth + 4);
            }
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (!state) return 0;
        POINT point{MessageX(lParam), MessageY(lParam)};
        if (state->toolbarOptions.pressedItem >= 0) {
            int pressed = state->toolbarOptions.pressedItem;
            RECT optionBounds = state->toolbarOptions.bounds;
            state->toolbarOptions.pressedItem = -1;
            ReleaseCapture();
            if (pressed == HitTestToolbarOptions(state->toolbarOptions, point)) {
                ToolbarOptionItem option = state->toolbarOptions.items[pressed];
                ApplyToolbarOption(state, option);
            }
            InvalidateArea(hwnd, optionBounds, 6);
            InvalidateArea(hwnd, state->toolbar.bounds, 4);
            return 0;
        }
        if (state->toolbarState.pressedItem >= 0) {
            int pressed = state->toolbarState.pressedItem;
            state->toolbarState.pressedItem = -1;
            ReleaseCapture();
            if (pressed == HitTestToolbar(state->toolbar, point)) {
                ExecuteToolbarAction(hwnd, state, state->toolbar.items[pressed].action);
            } else {
                InvalidateArea(hwnd, state->toolbar.bounds, 4);
            }
            return 0;
        }
        if (state->selecting) {
            state->selecting = false;
            state->dragCurrent = point;
            ReleaseCapture();
            RECT client = ClientBounds(hwnd);
            RECT selection = Normalize(state->dragStart, state->dragCurrent);
            selection.left = std::clamp<LONG>(selection.left, client.left, client.right);
            selection.top = std::clamp<LONG>(selection.top, client.top, client.bottom);
            selection.right = std::clamp<LONG>(selection.right, selection.left, client.right);
            selection.bottom = std::clamp<LONG>(selection.bottom, selection.top, client.bottom);
            if (selection.right - selection.left >= 4 && selection.bottom - selection.top >= 4) {
                state->selection = selection;
                state->selectionReady = true;
                state->toolbar = LayoutSelectionToolbar(selection, client.right, client.bottom);
                state->toolbarOptions = ToolbarOptions{};
                UpdateToolbarTooltip(state->tooltip, hwnd, state->toolbar, state->options.language);
            }
            InvalidateArea(hwnd, selection, 7);
            InvalidateArea(hwnd, state->toolbar.bounds, 6);
            return 0;
        }
        if (state->editing) {
            state->editCurrent = point;
            FinishEdit(hwnd, state);
            return 0;
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam == VK_RETURN && state && state->selectionReady) {
            Commit(hwnd, state, false);
            return 0;
        }
        if (state && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            if (wParam == 'C') {
                Commit(hwnd, state, false);
                return 0;
            }
            if (wParam == 'S') {
                Commit(hwnd, state, true);
                return 0;
            }
            if (wParam == 'Z') {
                state->document.Undo();
                Rebuild(state);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (wParam == 'Y') {
                state->document.Redo();
                Rebuild(state);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        if (state) PaintRegion(hwnd, dc, state, paint.rcPaint);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CANCELMODE:
        if (state) {
            state->selecting = false;
            state->editing = false;
            state->toolbarState.pressedItem = -1;
        }
        return 0;
    case WM_DESTROY:
        if (state && state->tooltip) {
            DestroyWindow(state->tooltip);
            state->tooltip = nullptr;
        }
        RestoreForeground(state);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool RegisterRegionClass(HINSTANCE instance) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = RegionWndProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    windowClass.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(101));
    windowClass.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kRegionWindowClass;
    ATOM atom = RegisterClassExW(&windowClass);
    return atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

RECT PrimaryMonitorRect() {
    HMONITOR monitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) return info.rcMonitor;
    return RECT{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
}

}  // namespace

int ShowRegionSelectionWindow(HINSTANCE instance,
                              const EditorOptions& options,
                              BgraImage image) {
    if (!RegisterRegionClass(instance) || !image.IsValid()) return 8;
    UINT imageWidth = image.width;
    UINT imageHeight = image.height;
    RegionState state(options, std::move(image));
    RECT monitor = PrimaryMonitorRect();
    int width = static_cast<int>(imageWidth);
    int height = static_cast<int>(imageHeight);
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                kRegionWindowClass,
                                GetEditorText(options.language, EditorTextId::RegionTitle),
                                WS_POPUP,
                                monitor.left,
                                monitor.top,
                                width,
                                height,
                                nullptr,
                                nullptr,
                                instance,
                                &state);
    if (!hwnd) return 8;
    if (!options.allowWindowCaptureForTests) SetWindowDisplayAffinity(hwnd, 0x00000011);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return state.accepted ? 0 : 2;
}

}  // namespace editor
