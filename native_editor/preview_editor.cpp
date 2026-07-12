#include "preview_editor.h"

#include "dib_surface.h"
#include "editor_clipboard.h"
#include "editor_text.h"
#include "editor_toolbar.h"
#include "editor_tooltips.h"
#include "mosaic_renderer.h"
#include "preview_viewport.h"
#include "wic_png.h"

#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace editor {

namespace {

const wchar_t kPreviewWindowClass[] = L"HdrSdrNativeEditorPreviewWindow";
const UINT kInitialCopyMessage = WM_APP + 41;
const int kTitleHeight = 42;
const int kToolbarAreaHeight = 82;
const COLORREF kBackground = RGB(10, 12, 13);
const COLORREF kBorder = RGB(55, 60, 64);

enum class CaptionButton {
    None,
    Maximize,
    Close,
};

struct PreviewState {
    PreviewState(const EditorOptions& launchOptions, BgraImage sourceImage)
        : options(launchOptions), document(std::move(sourceImage)) {
        surfaceReady = imageSurface.Load(document.Source());
        viewport.SetImage(document.Source().width, document.Source().height);
    }

    EditorOptions options;
    ImageDocument document;
    BgraImage rendered;
    DibSurface imageSurface;
    DibSurface frameSurface;
    PreviewViewport viewport;
    bool surfaceReady = false;
    RECT imageViewport{};
    RECT imageBounds{};
    ToolbarLayout toolbar;
    ToolbarVisualState toolbarState;
    ToolbarOptions toolbarOptions;
    int shapeStrokeWidth = 4;
    int penStrokeWidth = 6;
    int mosaicBrushSize = 28;
    bool editing = false;
    bool panning = false;
    POINT editStart{};
    POINT editCurrent{};
    POINT panLast{};
    std::vector<POINT> currentPoints;
    CaptionButton hoveredCaption = CaptionButton::None;
    CaptionButton pressedCaption = CaptionButton::None;
    std::wstring status;
    HWND tooltip = nullptr;
};

PreviewState* State(HWND hwnd) {
    return reinterpret_cast<PreviewState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

const BgraImage& CurrentImage(const PreviewState* state) {
    return state->rendered.IsValid() ? state->rendered : state->document.Source();
}

int MessageX(LPARAM value) { return static_cast<short>(LOWORD(value)); }
int MessageY(LPARAM value) { return static_cast<short>(HIWORD(value)); }

bool Contains(const RECT& rect, POINT point) {
    return point.x >= rect.left && point.x < rect.right &&
        point.y >= rect.top && point.y < rect.bottom;
}

RECT Normalize(POINT first, POINT second) {
    return RECT{std::min(first.x, second.x), std::min(first.y, second.y),
                std::max(first.x, second.x), std::max(first.y, second.y)};
}

void Layout(HWND hwnd, PreviewState* state) {
    if (!state) return;
    RECT client{};
    GetClientRect(hwnd, &client);
    RECT available{20, kTitleHeight + 10, client.right - 20,
                   std::max(kTitleHeight + 11L, client.bottom - kToolbarAreaHeight)};
    const BgraImage& image = CurrentImage(state);
    state->imageViewport = available;
    state->viewport.SetImage(image.width, image.height);
    state->viewport.SetAvailable(available);
    state->imageBounds = state->viewport.Bounds();
    state->toolbar = LayoutPreviewToolbar(client.right / 2,
                                          std::max(kTitleHeight + 4,
                                                   static_cast<int>(client.bottom - 72)));
    UpdateToolbarTooltip(state->tooltip, hwnd, state->toolbar, state->options.language);
}

POINT ClientToImage(const PreviewState* state, POINT point) {
    return state->viewport.ClientToImage(point);
}

POINT ImageToClient(const PreviewState* state, POINT point) {
    return state->viewport.ImageToClient(point);
}

RECT ClientRectToImage(const PreviewState* state, RECT requested) {
    RECT visible{};
    if (!IntersectRect(&visible, &state->imageBounds, &state->imageViewport)) return RECT{};
    requested.left = std::clamp<LONG>(requested.left, visible.left, visible.right);
    requested.top = std::clamp<LONG>(requested.top, visible.top, visible.bottom);
    requested.right = std::clamp<LONG>(requested.right, requested.left, visible.right);
    requested.bottom = std::clamp<LONG>(requested.bottom, requested.top, visible.bottom);
    POINT first = ClientToImage(state, POINT{requested.left, requested.top});
    POINT second = ClientToImage(state, POINT{requested.right, requested.bottom});
    const BgraImage& image = CurrentImage(state);
    second.x = std::min<LONG>(image.width, second.x + 1);
    second.y = std::min<LONG>(image.height, second.y + 1);
    return RECT{first.x, first.y, second.x, second.y};
}

void DrawText(HDC dc, const wchar_t* text, const RECT& rect, COLORREF color,
              UINT format, const wchar_t* fontName, int height = 16, int weight = FW_NORMAL) {
    HFONT font = CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontName);
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    RECT copy = rect;
    DrawTextW(dc, text, -1, &copy, format);
    SelectObject(dc, old);
    DeleteObject(font);
}

RECT CloseButtonRect(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);
    return RECT{client.right - 48, 0, client.right, kTitleHeight};
}

RECT MaximizeButtonRect(HWND hwnd) {
    RECT close = CloseButtonRect(hwnd);
    return RECT{close.left - 48, 0, close.left, kTitleHeight};
}

CaptionButton HitCaptionButton(HWND hwnd, POINT point) {
    if (Contains(CloseButtonRect(hwnd), point)) return CaptionButton::Close;
    if (Contains(MaximizeButtonRect(hwnd), point)) return CaptionButton::Maximize;
    return CaptionButton::None;
}

void DrawCaptionButtonBackground(HDC dc, const RECT& rect, CaptionButton button,
                                 const PreviewState* state) {
    if (!state || (state->hoveredCaption != button && state->pressedCaption != button)) return;
    COLORREF color = button == CaptionButton::Close
        ? RGB(128, 42, 48)
        : RGB(42, 47, 51);
    if (state->pressedCaption == button) {
        color = button == CaptionButton::Close ? RGB(105, 32, 38) : RGB(32, 36, 39);
    }
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void DrawMaximizeIcon(HDC dc, const RECT& rect, bool restored) {
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(190, 198, 204));
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    int cx = (rect.left + rect.right) / 2;
    int cy = (rect.top + rect.bottom) / 2;
    if (!restored) {
        Rectangle(dc, cx - 6, cy - 6, cx + 7, cy + 7);
    } else {
        Rectangle(dc, cx - 4, cy - 6, cx + 7, cy + 5);
        Rectangle(dc, cx - 7, cy - 3, cx + 4, cy + 8);
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawMosaicRectPreview(Gdiplus::Graphics* graphics, const PreviewState* state) {
    RECT clientRect = Normalize(state->editStart, state->editCurrent);
    RECT imageRect = ClientRectToImage(state, clientRect);
    if (imageRect.right <= imageRect.left || imageRect.bottom <= imageRect.top) return;
    int saved = graphics->Save();
    graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);
    graphics->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    graphics->SetClip(Gdiplus::Rect(clientRect.left, clientRect.top,
                                    clientRect.right - clientRect.left,
                                    clientRect.bottom - clientRect.top),
                      Gdiplus::CombineModeIntersect);
    const BgraImage& image = CurrentImage(state);
    int cell = std::max(8, state->mosaicBrushSize);
    for (int y = imageRect.top; y < imageRect.bottom; y += cell) {
        for (int x = imageRect.left; x < imageRect.right; x += cell) {
            RECT imageCell{x, y, std::min<int>(imageRect.right, x + cell),
                           std::min<int>(imageRect.bottom, y + cell)};
            COLORREF value = MosaicCellAverage(image, imageCell);
            POINT first = ImageToClient(state, POINT{imageCell.left, imageCell.top});
            POINT second = ImageToClient(state, POINT{imageCell.right, imageCell.bottom});
            Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(value),
                                                     GetGValue(value), GetBValue(value)));
            graphics->FillRectangle(&brush,
                                    static_cast<INT>(first.x),
                                    static_cast<INT>(first.y),
                                    static_cast<INT>(std::max<LONG>(1, second.x - first.x)),
                                    static_cast<INT>(std::max<LONG>(1, second.y - first.y)));
        }
    }
    graphics->Restore(saved);
}

void DrawCurrentEdit(Gdiplus::Graphics* graphics, const PreviewState* state) {
    if (!graphics || !state || !state->editing) return;
    if (state->toolbarState.mode == EditMode::Mosaic) {
        DrawMosaicRectPreview(graphics, state);
        return;
    }
    COLORREF value = AnnotationColorAt(state->toolbarState.annotationColorIndex);
    Gdiplus::Color color(255, GetRValue(value), GetGValue(value), GetBValue(value));
    double previewScale = (state->imageBounds.right - state->imageBounds.left) /
        static_cast<double>(std::max(1u, CurrentImage(state).width));
    int imageWidth = state->toolbarState.mode == EditMode::Pen
        ? state->penStrokeWidth
        : state->shapeStrokeWidth;
    float stroke = static_cast<float>(std::max(1.0, imageWidth * previewScale));
    Gdiplus::Pen pen(color, stroke);
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
        for (POINT imagePoint : state->currentPoints) {
            if (IsPathBreak(imagePoint)) {
                flush();
            } else {
                POINT clientPoint = ImageToClient(state, imagePoint);
                segment.emplace_back(clientPoint.x, clientPoint.y);
            }
        }
        flush();
        return;
    }
    RECT rect = Normalize(state->editStart, state->editCurrent);
    if (state->toolbarState.mode == EditMode::Ellipse) {
        graphics->DrawEllipse(&pen, static_cast<INT>(rect.left), static_cast<INT>(rect.top),
                              static_cast<INT>(rect.right - rect.left),
                              static_cast<INT>(rect.bottom - rect.top));
    } else {
        graphics->DrawRectangle(&pen, static_cast<INT>(rect.left), static_cast<INT>(rect.top),
                                static_cast<INT>(rect.right - rect.left),
                                static_cast<INT>(rect.bottom - rect.top));
    }
}

void Paint(HWND hwnd, HDC destinationDc, PreviewState* state, RECT dirty) {
    RECT client{};
    GetClientRect(hwnd, &client);
    int width = client.right;
    int height = client.bottom;
    if (!state->surfaceReady) state->surfaceReady = state->imageSurface.Load(CurrentImage(state));
    if (!state->surfaceReady || !state->frameSurface.Ensure(width, height) ||
        !IntersectRect(&dirty, &dirty, &client)) {
        return;
    }
    HDC bufferDc = state->frameSurface.Dc();
    int savedDc = SaveDC(bufferDc);
    IntersectClipRect(bufferDc, dirty.left, dirty.top, dirty.right, dirty.bottom);
    HBRUSH background = CreateSolidBrush(kBackground);
    FillRect(bufferDc, &dirty, background);
    DeleteObject(background);

    int imageDc = SaveDC(bufferDc);
    IntersectClipRect(bufferDc,
                      state->imageViewport.left, state->imageViewport.top,
                      state->imageViewport.right, state->imageViewport.bottom);
    int oldMode = SetStretchBltMode(bufferDc, HALFTONE);
    SetBrushOrgEx(bufferDc, 0, 0, nullptr);
    StretchBlt(bufferDc,
               state->imageBounds.left, state->imageBounds.top,
               state->imageBounds.right - state->imageBounds.left,
               state->imageBounds.bottom - state->imageBounds.top,
               state->imageSurface.Dc(), 0, 0,
               state->imageSurface.Width(), state->imageSurface.Height(), SRCCOPY);
    SetStretchBltMode(bufferDc, oldMode);
    HBRUSH imageBorder = CreateSolidBrush(kBorder);
    FrameRect(bufferDc, &state->imageBounds, imageBorder);
    DeleteObject(imageBorder);

    {
        Gdiplus::Graphics graphics(bufferDc);
        graphics.SetClip(Gdiplus::Rect(dirty.left, dirty.top,
                                       dirty.right - dirty.left,
                                       dirty.bottom - dirty.top));
        DrawCurrentEdit(&graphics, state);
        graphics.Flush(Gdiplus::FlushIntentionSync);
    }
    RestoreDC(bufferDc, imageDc);

    {
        Gdiplus::Graphics graphics(bufferDc);
        graphics.SetClip(Gdiplus::Rect(dirty.left, dirty.top,
                                       dirty.right - dirty.left,
                                       dirty.bottom - dirty.top));
        DrawEditorToolbar(&graphics, state->toolbar, state->toolbarState);
        DrawToolbarOptions(&graphics, state->toolbarOptions);
        graphics.Flush(Gdiplus::FlushIntentionSync);
    }

    RECT close = CloseButtonRect(hwnd);
    RECT maximize = MaximizeButtonRect(hwnd);
    DrawCaptionButtonBackground(bufferDc, maximize, CaptionButton::Maximize, state);
    DrawCaptionButtonBackground(bufferDc, close, CaptionButton::Close, state);
    RECT titleText{18, 0, maximize.left - 8, kTitleHeight};
    DrawText(bufferDc, GetEditorText(state->options.language, EditorTextId::PreviewTitle),
             titleText, RGB(225, 229, 232),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE,
             GetEditorFontName(state->options.language), 16, FW_SEMIBOLD);
    DrawMaximizeIcon(bufferDc, maximize, IsZoomed(hwnd) != FALSE);
    HPEN closePen = CreatePen(PS_SOLID, 2, RGB(190, 198, 204));
    HGDIOBJ oldPen = SelectObject(bufferDc, closePen);
    int cx = (close.left + close.right) / 2;
    int cy = (close.top + close.bottom) / 2;
    MoveToEx(bufferDc, cx - 6, cy - 6, nullptr);
    LineTo(bufferDc, cx + 6, cy + 6);
    MoveToEx(bufferDc, cx + 6, cy - 6, nullptr);
    LineTo(bufferDc, cx - 6, cy + 6);
    SelectObject(bufferDc, oldPen);
    DeleteObject(closePen);

    if (!state->status.empty()) {
        RECT status{20, client.bottom - kToolbarAreaHeight, client.right - 20,
                    client.bottom - 62};
        DrawText(bufferDc, state->status.c_str(), status, RGB(165, 174, 180),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS,
                 GetEditorFontName(state->options.language), 14);
    }
    BitBlt(destinationDc, dirty.left, dirty.top,
           dirty.right - dirty.left, dirty.bottom - dirty.top,
           bufferDc, dirty.left, dirty.top, SRCCOPY);
    RestoreDC(bufferDc, savedDc);
}

void Rebuild(HWND hwnd, PreviewState* state) {
    state->rendered = state->document.Render();
    state->surfaceReady = state->imageSurface.Load(CurrentImage(state));
    state->toolbarState.preset = state->document.GetAdjustmentPreset();
    Layout(hwnd, state);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void SetStatus(HWND hwnd, PreviewState* state, const std::wstring& status) {
    if (!state) return;
    state->status = status;
    InvalidateRect(hwnd, nullptr, FALSE);
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

bool Copy(HWND hwnd, PreviewState* state, bool closeAfter) {
    std::wstring error;
    if (!CopyImageToClipboard(hwnd, CurrentImage(state), &error)) {
        SetStatus(hwnd, state, error);
        return false;
    }
    if (closeAfter) DestroyWindow(hwnd);
    else SetStatus(hwnd, state,
                   GetEditorText(state->options.language, EditorTextId::CopiedStatus));
    return true;
}

void Save(HWND hwnd, PreviewState* state) {
    std::wstring path;
    if (!PromptSave(hwnd, state->options.language, state->options.outputPath, &path)) return;
    std::wstring error;
    SetStatus(hwnd, state,
              SavePng(path, CurrentImage(state), &error)
                  ? GetEditorText(state->options.language, EditorTextId::SavedStatus)
                  : error);
}

const ToolbarItem* FindToolbarItem(const ToolbarLayout& toolbar, ToolbarAction action) {
    for (const ToolbarItem& item : toolbar.items) {
        if (item.action == action) return &item;
    }
    return nullptr;
}

int CurrentToolSize(const PreviewState* state, EditMode mode) {
    if (mode == EditMode::Marker || mode == EditMode::Ellipse) return state->shapeStrokeWidth;
    if (mode == EditMode::Pen) return state->penStrokeWidth;
    if (mode == EditMode::Mosaic) return state->mosaicBrushSize;
    return 0;
}

void SetTool(HWND hwnd, PreviewState* state, EditMode mode, ToolbarAction action) {
    bool alreadySelected = state->toolbarState.mode == mode;
    state->toolbarState.mode = mode;
    state->toolbarOptions.visible = false;
    if (alreadySelected) {
        const ToolbarItem* anchor = FindToolbarItem(state->toolbar, action);
        RECT client{};
        GetClientRect(hwnd, &client);
        if (anchor) {
            state->toolbarOptions = LayoutSizeOptions(action, anchor->rect, state->toolbar.bounds,
                CurrentToolSize(state, mode), client.right, client.bottom);
        }
    }
}

void ApplyToolbarOption(PreviewState* state, const ToolbarOptionItem& option) {
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

void ExecuteToolbarAction(HWND hwnd, PreviewState* state, ToolbarAction action) {
    switch (action) {
    case ToolbarAction::Cancel: DestroyWindow(hwnd); return;
    case ToolbarAction::ToolMarker: SetTool(hwnd, state, EditMode::Marker, action); break;
    case ToolbarAction::ToolEllipse: SetTool(hwnd, state, EditMode::Ellipse, action); break;
    case ToolbarAction::ToolPen: SetTool(hwnd, state, EditMode::Pen, action); break;
    case ToolbarAction::ToolMosaic: SetTool(hwnd, state, EditMode::Mosaic, action); break;
    case ToolbarAction::Color: {
        const ToolbarItem* anchor = FindToolbarItem(state->toolbar, action);
        RECT client{};
        GetClientRect(hwnd, &client);
        if (anchor) {
            state->toolbarOptions = LayoutColorOptions(anchor->rect, state->toolbar.bounds,
                state->toolbarState.annotationColorIndex, client.right, client.bottom);
        }
        break;
    }
    case ToolbarAction::Undo: state->document.Undo(); Rebuild(hwnd, state); return;
    case ToolbarAction::Redo: state->document.Redo(); Rebuild(hwnd, state); return;
    case ToolbarAction::Reset:
        state->document.Reset();
        state->toolbarState.mode = EditMode::None;
        state->toolbarState.annotationColorIndex = 0;
        Rebuild(hwnd, state);
        return;
    case ToolbarAction::PresetLow:
        state->document.SetAdjustmentPreset(AdjustmentPreset::Low);
        Rebuild(hwnd, state);
        return;
    case ToolbarAction::PresetBalanced:
        state->document.SetAdjustmentPreset(AdjustmentPreset::Balanced);
        Rebuild(hwnd, state);
        return;
    case ToolbarAction::PresetHigh:
        state->document.SetAdjustmentPreset(AdjustmentPreset::High);
        Rebuild(hwnd, state);
        return;
    case ToolbarAction::Save: Save(hwnd, state); return;
    case ToolbarAction::Copy: Copy(hwnd, state, true); return;
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

void FinishEdit(HWND hwnd, PreviewState* state) {
    if (!state || !state->editing) return;
    state->editing = false;
    ReleaseCapture();
    EditOperation operation;
    operation.color = AnnotationColorAt(state->toolbarState.annotationColorIndex);
    if (state->toolbarState.mode == EditMode::Pen) {
        size_t pointCount = 0;
        for (POINT point : state->currentPoints) {
            if (!IsPathBreak(point)) ++pointCount;
        }
        if (pointCount < 2) {
            state->currentPoints.clear();
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        operation.type = EditOperationType::Pen;
        operation.points = state->currentPoints;
        operation.strokeWidth = state->penStrokeWidth;
    } else {
        RECT clientRect = Normalize(state->editStart, state->editCurrent);
        operation.rect = ClientRectToImage(state, clientRect);
        if (operation.rect.right - operation.rect.left < 2 ||
            operation.rect.bottom - operation.rect.top < 2) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        if (state->toolbarState.mode == EditMode::Ellipse) {
            operation.type = EditOperationType::Ellipse;
            operation.strokeWidth = state->shapeStrokeWidth;
        } else if (state->toolbarState.mode == EditMode::Mosaic) {
            operation.type = EditOperationType::Mosaic;
            operation.strokeWidth = state->mosaicBrushSize;
        } else {
            operation.type = EditOperationType::Marker;
            operation.strokeWidth = state->shapeStrokeWidth;
        }
    }
    state->document.AddOperation(operation);
    state->currentPoints.clear();
    Rebuild(hwnd, state);
}

void ApplyDarkFrame(HWND hwnd) {
    BOOL enabled = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &enabled, sizeof(enabled));
    DwmSetWindowAttribute(hwnd, 19, &enabled, sizeof(enabled));
    COLORREF border = kBorder;
    DwmSetWindowAttribute(hwnd, 34, &border, sizeof(border));
}

void ApplyMonitorWorkArea(HWND hwnd, MINMAXINFO* info) {
    if (!info) return;
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo)) return;
    info->ptMaxPosition.x = monitorInfo.rcWork.left - monitorInfo.rcMonitor.left;
    info->ptMaxPosition.y = monitorInfo.rcWork.top - monitorInfo.rcMonitor.top;
    info->ptMaxSize.x = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
    info->ptMaxSize.y = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
    info->ptMaxTrackSize = info->ptMaxSize;
}

void ToggleMaximize(HWND hwnd) {
    ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
}

LRESULT HitTestWindow(HWND hwnd, POINT screenPoint) {
    RECT window{};
    GetWindowRect(hwnd, &window);
    int border = std::max(6u, GetDpiForWindow(hwnd) * 6 / 96);
    bool left = screenPoint.x < window.left + border;
    bool right = screenPoint.x >= window.right - border;
    bool top = screenPoint.y < window.top + border;
    bool bottom = screenPoint.y >= window.bottom - border;
    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;
    POINT client = screenPoint;
    ScreenToClient(hwnd, &client);
    if (client.y < kTitleHeight && HitCaptionButton(hwnd, client) == CaptionButton::None) {
        return HTCAPTION;
    }
    return HTCLIENT;
}

LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PreviewState* state = State(hwnd);
    switch (message) {
    case WM_NCCREATE: {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    case WM_CREATE:
        ApplyDarkFrame(hwnd);
        if (state) state->tooltip = CreateToolbarTooltip(hwnd);
        return 0;
    case WM_NCCALCSIZE:
        if (wParam) return 0;
        break;
    case WM_NCHITTEST: {
        POINT point{MessageX(lParam), MessageY(lParam)};
        return HitTestWindow(hwnd, point);
    }
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        ApplyMonitorWorkArea(hwnd, info);
        info->ptMinTrackSize.x = 900;
        info->ptMinTrackSize.y = 520;
        return 0;
    }
    case WM_SIZE:
        if (state) Layout(hwnd, state);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        if (!state) return 0;
        POINT point{MessageX(lParam), MessageY(lParam)};
        CaptionButton caption = HitCaptionButton(hwnd, point);
        if (caption != CaptionButton::None) {
            state->pressedCaption = caption;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        int optionHit = HitTestToolbarOptions(state->toolbarOptions, point);
        if (optionHit >= 0) {
            state->toolbarOptions.pressedItem = optionHit;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (state->toolbarOptions.visible) {
            state->toolbarOptions = ToolbarOptions{};
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        int toolbarHit = HitTestToolbar(state->toolbar, point);
        if (toolbarHit >= 0) {
            state->toolbarState.pressedItem = toolbarHit;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (state->toolbarState.mode != EditMode::None &&
            Contains(state->imageViewport, point) && Contains(state->imageBounds, point)) {
            state->editing = true;
            state->editStart = point;
            state->editCurrent = point;
            state->currentPoints.clear();
            if (state->toolbarState.mode == EditMode::Pen) {
                state->currentPoints.push_back(ClientToImage(state, point));
            }
            SetCapture(hwnd);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!state) return 0;
        POINT point{MessageX(lParam), MessageY(lParam)};
        CaptionButton caption = HitCaptionButton(hwnd, point);
        if (caption != state->hoveredCaption) {
            state->hoveredCaption = caption;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        TRACKMOUSEEVENT tracking{};
        tracking.cbSize = sizeof(tracking);
        tracking.dwFlags = TME_LEAVE;
        tracking.hwndTrack = hwnd;
        TrackMouseEvent(&tracking);
        if (state->panning) {
            int deltaX = point.x - state->panLast.x;
            int deltaY = point.y - state->panLast.y;
            state->panLast = point;
            if (state->viewport.PanBy(deltaX, deltaY)) {
                state->imageBounds = state->viewport.Bounds();
                InvalidateRect(hwnd, &state->imageViewport, FALSE);
            }
            return 0;
        }
        int optionHover = HitTestToolbarOptions(state->toolbarOptions, point);
        if (optionHover != state->toolbarOptions.hoveredItem) {
            state->toolbarOptions.hoveredItem = optionHover;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        int hover = optionHover >= 0 ? -1 : HitTestToolbar(state->toolbar, point);
        if (hover != state->toolbarState.hoveredItem) {
            state->toolbarState.hoveredItem = hover;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        if (state->editing) {
            state->editCurrent = point;
            if (state->toolbarState.mode == EditMode::Pen) {
                if (Contains(state->imageViewport, point) && Contains(state->imageBounds, point)) {
                    POINT imagePoint = ClientToImage(state, point);
                    if (state->currentPoints.empty() || IsPathBreak(state->currentPoints.back())) {
                        state->currentPoints.push_back(imagePoint);
                    } else {
                        POINT previous = state->currentPoints.back();
                        int dx = imagePoint.x - previous.x;
                        int dy = imagePoint.y - previous.y;
                        if (dx * dx + dy * dy >= 4) state->currentPoints.push_back(imagePoint);
                    }
                } else if (!state->currentPoints.empty() && !IsPathBreak(state->currentPoints.back())) {
                    state->currentPoints.push_back(PathBreakPoint());
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (!state) return 0;
        POINT point{MessageX(lParam), MessageY(lParam)};
        if (state->pressedCaption != CaptionButton::None) {
            CaptionButton pressed = state->pressedCaption;
            state->pressedCaption = CaptionButton::None;
            ReleaseCapture();
            if (pressed == HitCaptionButton(hwnd, point)) {
                if (pressed == CaptionButton::Close) DestroyWindow(hwnd);
                else ToggleMaximize(hwnd);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (state->toolbarOptions.pressedItem >= 0) {
            int pressed = state->toolbarOptions.pressedItem;
            state->toolbarOptions.pressedItem = -1;
            ReleaseCapture();
            if (pressed == HitTestToolbarOptions(state->toolbarOptions, point)) {
                ToolbarOptionItem option = state->toolbarOptions.items[pressed];
                ApplyToolbarOption(state, option);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (state->toolbarState.pressedItem >= 0) {
            int pressed = state->toolbarState.pressedItem;
            state->toolbarState.pressedItem = -1;
            ReleaseCapture();
            if (pressed == HitTestToolbar(state->toolbar, point)) {
                ExecuteToolbarAction(hwnd, state, state->toolbar.items[pressed].action);
            } else {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        if (state->editing) {
            state->editCurrent = point;
            FinishEdit(hwnd, state);
        }
        return 0;
    }
    case WM_MBUTTONDOWN: {
        if (!state || state->editing || !state->viewport.IsZoomed()) return 0;
        POINT point{MessageX(lParam), MessageY(lParam)};
        if (!Contains(state->imageViewport, point) || !Contains(state->imageBounds, point)) return 0;
        state->panning = true;
        state->panLast = point;
        SetCapture(hwnd);
        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
        return 0;
    }
    case WM_MBUTTONUP:
        if (state && state->panning) {
            state->panning = false;
            ReleaseCapture();
        }
        return 0;
    case WM_MOUSEWHEEL: {
        if (!state || state->editing || state->panning) return 0;
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &point);
        if (Contains(state->imageViewport, point) &&
            state->viewport.ZoomAt(point, GET_WHEEL_DELTA_WPARAM(wParam))) {
            state->imageBounds = state->viewport.Bounds();
            InvalidateRect(hwnd, &state->imageViewport, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        if (state && state->hoveredCaption != CaptionButton::None) {
            state->hoveredCaption = CaptionButton::None;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (state) {
            state->panning = false;
            state->pressedCaption = CaptionButton::None;
        }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && state) {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(hwnd, &point);
            LPCWSTR cursor = IDC_ARROW;
            if (state->panning ||
                (state->viewport.IsZoomed() && Contains(state->imageViewport, point) &&
                 (GetKeyState(VK_MBUTTON) & 0x8000) != 0)) {
                cursor = IDC_SIZEALL;
            } else if (HitTestToolbar(state->toolbar, point) >= 0) {
                cursor = IDC_HAND;
            }
            SetCursor(LoadCursorW(nullptr, cursor));
            return TRUE;
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (state && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            if (wParam == 'C') { Copy(hwnd, state, false); return 0; }
            if (wParam == 'S') { Save(hwnd, state); return 0; }
            if (wParam == 'Z') { state->document.Undo(); Rebuild(hwnd, state); return 0; }
            if (wParam == 'Y') { state->document.Redo(); Rebuild(hwnd, state); return 0; }
        }
        break;
    case kInitialCopyMessage:
        if (state && !state->options.skipInitialCopy) Copy(hwnd, state, false);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        if (state) Paint(hwnd, dc, state, paint.rcPaint);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        if (state && state->tooltip) {
            DestroyWindow(state->tooltip);
            state->tooltip = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool RegisterPreviewClass(HINSTANCE instance) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = PreviewWndProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    windowClass.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(101));
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kPreviewWindowClass;
    ATOM atom = RegisterClassExW(&windowClass);
    return atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

}  // namespace

int ShowPreviewEditorWindow(HINSTANCE instance,
                            const EditorOptions& options,
                            BgraImage image) {
    if (!RegisterPreviewClass(instance) || !image.IsValid()) return 8;
    PreviewState state(options, std::move(image));
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int workWidth = std::max(920L, work.right - work.left);
    int workHeight = std::max(540L, work.bottom - work.top);
    int width = std::min(1180, workWidth - 48);
    int height = std::min(760, workHeight - 48);
    int x = work.left + (workWidth - width) / 2;
    int y = work.top + (workHeight - height) / 2;
    DWORD style = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW,
                                kPreviewWindowClass,
                                GetEditorText(options.language, EditorTextId::PreviewTitle),
                                style,
                                x, y, width, height,
                                nullptr, nullptr, instance, &state);
    if (!hwnd) return 8;
    Layout(hwnd, &state);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    PostMessageW(hwnd, kInitialCopyMessage, 0, 0);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}

}  // namespace editor
