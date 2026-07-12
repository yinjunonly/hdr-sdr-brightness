#include "clipboard_output.h"

#include <cstdio>
#include <cstring>

namespace native_capture {

namespace {

bool TryOpenClipboardWithOwner(HWND owner, DWORD* lastError) {
    if (OpenClipboard(owner)) return true;
    if (lastError) *lastError = GetLastError();
    return false;
}

bool OpenClipboardWithRetry(DWORD* finalError) {
    DWORD lastError = ERROR_SUCCESS;
    HWND owners[] = {
        nullptr,
        GetForegroundWindow(),
        GetConsoleWindow()
    };

    for (int attempt = 0; attempt < 40; ++attempt) {
        for (HWND owner : owners) {
            if (TryOpenClipboardWithOwner(owner, &lastError)) {
                if (finalError) *finalError = ERROR_SUCCESS;
                return true;
            }
        }

        Sleep(attempt < 10 ? 40 : 100);
    }

    if (finalError) *finalError = lastError;
    return false;
}

}  // namespace

bool CopyBgraToClipboard(UINT width, UINT height, const std::vector<BYTE>& bgra) {
    size_t pixelBytes = static_cast<size_t>(width) * height * 4;
    if (width == 0 || height == 0 || bgra.size() < pixelBytes) {
        std::fprintf(stderr, "Clipboard bitmap is empty.\n");
        return false;
    }
    if (pixelBytes > 0xffffffffu) {
        std::fprintf(stderr, "Clipboard bitmap is too large.\n");
        return false;
    }

    size_t dibBytes = sizeof(BITMAPINFOHEADER) + pixelBytes;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, dibBytes);
    if (!memory) {
        std::fprintf(stderr, "GlobalAlloc failed for clipboard bitmap.\n");
        return false;
    }

    void* raw = GlobalLock(memory);
    if (!raw) {
        GlobalFree(memory);
        std::fprintf(stderr, "GlobalLock failed for clipboard bitmap.\n");
        return false;
    }

    BITMAPINFOHEADER header = {};
    header.biSize = sizeof(header);
    header.biWidth = static_cast<LONG>(width);
    header.biHeight = -static_cast<LONG>(height);
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    header.biSizeImage = static_cast<DWORD>(pixelBytes);

    std::memcpy(raw, &header, sizeof(header));
    std::memcpy(static_cast<BYTE*>(raw) + sizeof(header), bgra.data(), pixelBytes);
    GlobalUnlock(memory);

    DWORD openError = ERROR_SUCCESS;
    if (!OpenClipboardWithRetry(&openError)) {
        GlobalFree(memory);
        std::fprintf(stderr, "OpenClipboard failed: %lu.\n", static_cast<unsigned long>(openError));
        return false;
    }

    EmptyClipboard();
    if (!SetClipboardData(CF_DIB, memory)) {
        CloseClipboard();
        GlobalFree(memory);
        std::fprintf(stderr, "SetClipboardData(CF_DIB) failed.\n");
        return false;
    }

    CloseClipboard();
    return true;
}

}  // namespace native_capture
