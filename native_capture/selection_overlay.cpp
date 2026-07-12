#include "selection_overlay.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace native_capture {

namespace {

const wchar_t kSelectionWindowClass[] = L"HdrSdrNativeSelectionOverlay";
const int kCrossCursorId = 32515;

struct SelectionState {
    RECT monitorRect{};
    HBITMAP preview = nullptr;
    int previewWidth = 0;
    int previewHeight = 0;
    POINT start{};
    POINT current{};
    RECT selected{};
    bool dragging = false;
    bool accepted = false;
    bool hasSelection = false;
    HDC backBufferDc = nullptr;
    HBITMAP backBuffer = nullptr;
    HGDIOBJ oldBackBuffer = nullptr;
    int backBufferWidth = 0;
    int backBufferHeight = 0;
};

RECT NormalizeRect(POINT a, POINT b) {
    RECT rect{};
    rect.left = std::min(a.x, b.x);
    rect.top = std::min(a.y, b.y);
    rect.right = std::max(a.x, b.x);
    rect.bottom = std::max(a.y, b.y);
    return rect;
}

int LParamX(LPARAM value) {
    return static_cast<short>(LOWORD(value));
}

int LParamY(LPARAM value) {
    return static_cast<short>(HIWORD(value));
}

void FillDim(HDC dc, const RECT& rect) {
    if (rect.right <= rect.left || rect.bottom <= rect.top) return;

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    HDC memory = CreateCompatibleDC(dc);
    if (!memory) return;

    HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
    if (!bitmap) {
        DeleteDC(memory);
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(memory, bitmap);
    RECT fill{0, 0, width, height};
    HBRUSH dim = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(memory, &fill, dim);
    DeleteObject(dim);

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 145;
    AlphaBlend(dc, rect.left, rect.top, width, height, memory, 0, 0, width, height, blend);

    SelectObject(memory, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
}

void DrawPreview(HDC dc, const SelectionState* state, const RECT& client) {
    if (!state || !state->preview) {
        HBRUSH background = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(dc, &client, background);
        DeleteObject(background);
        return;
    }

    HDC memoryDc = CreateCompatibleDC(dc);
    if (!memoryDc) return;

    HGDIOBJ oldBitmap = SelectObject(memoryDc, state->preview);
    BitBlt(dc, 0, 0, state->previewWidth, state->previewHeight, memoryDc, 0, 0, SRCCOPY);
    SelectObject(memoryDc, oldBitmap);
    DeleteDC(memoryDc);
}

void DimOutsideSelection(HDC dc, const RECT& client, const RECT* selected) {
    if (!selected) {
        FillDim(dc, client);
        return;
    }

    RECT top{client.left, client.top, client.right, selected->top};
    RECT bottom{client.left, selected->bottom, client.right, client.bottom};
    RECT left{client.left, selected->top, selected->left, selected->bottom};
    RECT right{selected->right, selected->top, client.right, selected->bottom};
    FillDim(dc, top);
    FillDim(dc, bottom);
    FillDim(dc, left);
    FillDim(dc, right);
}

void DrawSelectionChrome(HDC dc, const RECT& client, const RECT& rect) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;

    RECT inner = rect;
    InflateRect(&inner, -1, -1);
    if (inner.right > inner.left && inner.bottom > inner.top) {
        HPEN clearEdge = CreatePen(PS_SOLID, 1, RGB(245, 252, 255));
        HGDIOBJ oldPen = SelectObject(dc, clearEdge);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, inner.left, inner.top, inner.right, inner.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(clearEdge);
    }

    HPEN border = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    HGDIOBJ oldPen = SelectObject(dc, border);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(border);

    HPEN accent = CreatePen(PS_SOLID, 1, RGB(0, 153, 255));
    oldPen = SelectObject(dc, accent);
    Rectangle(dc, rect.left + 2, rect.top + 2, rect.right - 2, rect.bottom - 2);
    SelectObject(dc, oldPen);
    DeleteObject(accent);

    int handle = 7;
    HBRUSH handleBrush = CreateSolidBrush(RGB(255, 255, 255));
    RECT handles[] = {
        {rect.left - handle / 2, rect.top - handle / 2, rect.left + handle / 2, rect.top + handle / 2},
        {rect.right - handle / 2, rect.top - handle / 2, rect.right + handle / 2, rect.top + handle / 2},
        {rect.left - handle / 2, rect.bottom - handle / 2, rect.left + handle / 2, rect.bottom + handle / 2},
        {rect.right - handle / 2, rect.bottom - handle / 2, rect.right + handle / 2, rect.bottom + handle / 2}
    };
    for (RECT handleRect : handles) {
        FillRect(dc, &handleRect, handleBrush);
    }
    DeleteObject(handleBrush);

    wchar_t label[64] = {};
    swprintf(label, 64, L"%d x %d", width, height);
    SIZE textSize{};
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT)));
    GetTextExtentPoint32W(dc, label, lstrlenW(label), &textSize);
    int labelX = rect.left;
    int labelY = rect.top - textSize.cy - 12;
    if (labelY < 8) labelY = rect.bottom + 8;
    if (labelX + textSize.cx + 18 > client.right) labelX = client.right - textSize.cx - 18;
    if (labelX < 8) labelX = 8;

    RECT labelRect{labelX, labelY, labelX + textSize.cx + 18, labelY + textSize.cy + 8};
    HBRUSH labelBrush = CreateSolidBrush(RGB(18, 22, 26));
    FillRect(dc, &labelRect, labelBrush);
    DeleteObject(labelBrush);
    HPEN labelBorder = CreatePen(PS_SOLID, 1, RGB(110, 190, 255));
    oldPen = SelectObject(dc, labelBorder);
    oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, labelRect.left, labelRect.top, labelRect.right, labelRect.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(labelBorder);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(235, 245, 255));
    RECT textRect{labelRect.left + 9, labelRect.top + 4, labelRect.right - 9, labelRect.bottom - 4};
    DrawTextW(dc, label, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldFont);
}

void DrawSelection(HDC dc, const SelectionState* state) {
    RECT client{};
    HWND hwnd = WindowFromDC(dc);
    GetClientRect(hwnd, &client);

    DrawPreview(dc, state, client);

    if (!state || (!state->dragging && !state->hasSelection)) {
        DimOutsideSelection(dc, client, nullptr);
        return;
    }
    RECT rect = state->dragging ? NormalizeRect(state->start, state->current) : state->selected;
    DimOutsideSelection(dc, client, &rect);
    DrawSelectionChrome(dc, client, rect);
}

bool EnsureBackBuffer(HDC dc, SelectionState* state, int width, int height) {
    if (!state || width <= 0 || height <= 0) return false;
    if (state->backBufferDc && state->backBuffer &&
        state->backBufferWidth == width && state->backBufferHeight == height) {
        return true;
    }

    if (state->backBufferDc) {
        if (state->oldBackBuffer) SelectObject(state->backBufferDc, state->oldBackBuffer);
        if (state->backBuffer) DeleteObject(state->backBuffer);
        DeleteDC(state->backBufferDc);
    }
    state->backBufferDc = CreateCompatibleDC(dc);
    state->backBuffer = state->backBufferDc ? CreateCompatibleBitmap(dc, width, height) : nullptr;
    if (!state->backBufferDc || !state->backBuffer) {
        if (state->backBuffer) DeleteObject(state->backBuffer);
        if (state->backBufferDc) DeleteDC(state->backBufferDc);
        state->backBufferDc = nullptr;
        state->backBuffer = nullptr;
        state->oldBackBuffer = nullptr;
        return false;
    }
    state->oldBackBuffer = SelectObject(state->backBufferDc, state->backBuffer);
    state->backBufferWidth = width;
    state->backBufferHeight = height;
    return true;
}

void DeleteBackBuffer(SelectionState* state) {
    if (!state || !state->backBufferDc) return;
    if (state->oldBackBuffer) SelectObject(state->backBufferDc, state->oldBackBuffer);
    if (state->backBuffer) DeleteObject(state->backBuffer);
    DeleteDC(state->backBufferDc);
    state->backBufferDc = nullptr;
    state->backBuffer = nullptr;
    state->oldBackBuffer = nullptr;
}

void DrawSelectionBuffered(HWND hwnd, HDC dc, SelectionState* state) {
    RECT client{};
    GetClientRect(hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (!EnsureBackBuffer(dc, state, width, height)) {
        DrawSelection(dc, state);
        return;
    }
    DrawSelection(state->backBufferDc, state);
    BitBlt(dc, 0, 0, width, height, state->backBufferDc, 0, 0, SRCCOPY);
}

bool AcceptCurrentSelection(HWND hwnd, SelectionState* state) {
    if (!state) return false;

    RECT selected = state->dragging ? NormalizeRect(state->start, state->current) : state->selected;
    int width = selected.right - selected.left;
    int height = selected.bottom - selected.top;
    if (width < 4 || height < 4) return false;

    state->selected = selected;
    state->accepted = true;
    state->hasSelection = true;
    if (state->dragging) {
        state->dragging = false;
        ReleaseCapture();
    }
    DestroyWindow(hwnd);
    return true;
}

LRESULT CALLBACK SelectionWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SelectionState* state = reinterpret_cast<SelectionState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return 0;
    }
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(kCrossCursorId)));
        return TRUE;
    case WM_LBUTTONDOWN:
        if (state) {
            state->dragging = true;
            state->accepted = false;
            state->hasSelection = false;
            state->start.x = LParamX(lParam);
            state->start.y = LParamY(lParam);
            state->current = state->start;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state && state->dragging) {
            state->current.x = LParamX(lParam);
            state->current.y = LParamY(lParam);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state && state->dragging) {
            state->current.x = LParamX(lParam);
            state->current.y = LParamY(lParam);
            AcceptCurrentSelection(hwnd, state);
        }
        return 0;
    case WM_RBUTTONDOWN:
    case WM_CANCELMODE:
        if (state) state->accepted = false;
        DestroyWindow(hwnd);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (state) state->accepted = false;
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam == VK_RETURN || wParam == VK_SPACE) {
            if (AcceptCurrentSelection(hwnd, state)) return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        DrawSelectionBuffered(hwnd, dc, state);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool RegisterSelectionWindowClass(HINSTANCE instance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SelectionWndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(kCrossCursorId));
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kSelectionWindowClass;
    ATOM atom = RegisterClassExW(&wc);
    return atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

RECT PrimaryMonitorRect() {
    HMONITOR monitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) return info.rcMonitor;

    RECT fallback{};
    fallback.left = 0;
    fallback.top = 0;
    fallback.right = GetSystemMetrics(SM_CXSCREEN);
    fallback.bottom = GetSystemMetrics(SM_CYSCREEN);
    return fallback;
}

HBITMAP CreatePreviewBitmap(UINT width, UINT height, const BYTE* bgra) {
    if (width == 0 || height == 0 || !bgra) return nullptr;

    HDC screen = GetDC(nullptr);
    if (!screen) return nullptr;

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = static_cast<LONG>(width);
    info.bmiHeader.biHeight = -static_cast<LONG>(height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!bitmap || !pixels) {
        if (bitmap) DeleteObject(bitmap);
        return nullptr;
    }
    std::memcpy(pixels, bgra, static_cast<size_t>(width) * height * 4);
    return bitmap;
}

void DeletePreviewBitmap(SelectionState* state) {
    if (state && state->preview) {
        DeleteObject(state->preview);
        state->preview = nullptr;
    }
}

}  // namespace

bool SelectPrimaryMonitorRegion(UINT previewWidth, UINT previewHeight, const BYTE* previewBgra, RECT* region) {
    if (!region || previewWidth == 0 || previewHeight == 0 || !previewBgra) return false;

    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!RegisterSelectionWindowClass(instance)) return false;

    SelectionState state{};
    state.monitorRect = PrimaryMonitorRect();
    int width = static_cast<int>(previewWidth);
    int height = static_cast<int>(previewHeight);
    if (width <= 0 || height <= 0) return false;
    state.monitorRect.right = state.monitorRect.left + width;
    state.monitorRect.bottom = state.monitorRect.top + height;
    state.previewWidth = width;
    state.previewHeight = height;
    state.preview = CreatePreviewBitmap(previewWidth, previewHeight, previewBgra);

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                kSelectionWindowClass,
                                L"HDR SDR Region Selection",
                                WS_POPUP,
                                state.monitorRect.left,
                                state.monitorRect.top,
                                width,
                                height,
                                nullptr,
                                nullptr,
                                instance,
                                &state);
    if (!hwnd) {
        DeletePreviewBitmap(&state);
        return false;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (!state.accepted) {
        DeleteBackBuffer(&state);
        DeletePreviewBitmap(&state);
        return false;
    }

    region->left = state.selected.left;
    region->top = state.selected.top;
    region->right = state.selected.right;
    region->bottom = state.selected.bottom;
    DeleteBackBuffer(&state);
    DeletePreviewBitmap(&state);
    return true;
}

}  // namespace native_capture
