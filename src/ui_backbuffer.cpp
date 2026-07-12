#include "ui_backbuffer.h"

namespace ui_backbuffer {

void Draw(HWND window, HDC target, DrawCallback callback) {
    RECT client = {};
    GetClientRect(window, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;

    HDC buffer = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    if (!buffer || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (buffer) DeleteDC(buffer);
        callback(window, target);
        return;
    }

    HGDIOBJ previousBitmap = SelectObject(buffer, bitmap);
    callback(window, buffer);
    BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
    SelectObject(buffer, previousBitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
}

}  // namespace ui_backbuffer
