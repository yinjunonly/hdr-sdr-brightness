#include "editor_clipboard.h"

#include "wic_png.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace editor {

namespace {

bool Fail(const wchar_t* message, std::wstring* error) {
    if (error) *error = message;
    return false;
}

HGLOBAL AllocateBytes(const void* bytes, size_t size) {
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory) return nullptr;
    void* raw = GlobalLock(memory);
    if (!raw) {
        GlobalFree(memory);
        return nullptr;
    }
    std::memcpy(raw, bytes, size);
    GlobalUnlock(memory);
    return memory;
}

HGLOBAL CreateCompatibleDib(const BgraImage& image) {
    uint64_t rowBytes64 = static_cast<uint64_t>(image.width) * 3;
    uint64_t stride64 = (rowBytes64 + 3) & ~uint64_t(3);
    uint64_t pixels64 = stride64 * image.height;
    uint64_t total64 = sizeof(BITMAPINFOHEADER) + pixels64;
    if (pixels64 > std::numeric_limits<DWORD>::max() || total64 > SIZE_MAX) return nullptr;

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(total64));
    if (!memory) return nullptr;
    BYTE* raw = static_cast<BYTE*>(GlobalLock(memory));
    if (!raw) {
        GlobalFree(memory);
        return nullptr;
    }

    BITMAPINFOHEADER header{};
    header.biSize = sizeof(header);
    header.biWidth = static_cast<LONG>(image.width);
    header.biHeight = static_cast<LONG>(image.height);
    header.biPlanes = 1;
    header.biBitCount = 24;
    header.biCompression = BI_RGB;
    header.biSizeImage = static_cast<DWORD>(pixels64);
    std::memcpy(raw, &header, sizeof(header));

    BYTE* pixels = raw + sizeof(header);
    size_t sourceStride = static_cast<size_t>(image.width) * 4;
    size_t destinationStride = static_cast<size_t>(stride64);
    for (UINT y = 0; y < image.height; ++y) {
        const BYTE* source = image.pixels.data() + static_cast<size_t>(y) * sourceStride;
        BYTE* destination = pixels + static_cast<size_t>(image.height - 1 - y) * destinationStride;
        std::memset(destination, 0, destinationStride);
        for (UINT x = 0; x < image.width; ++x) {
            destination[x * 3] = source[x * 4];
            destination[x * 3 + 1] = source[x * 4 + 1];
            destination[x * 3 + 2] = source[x * 4 + 2];
        }
    }
    GlobalUnlock(memory);
    return memory;
}

bool OpenClipboardWithRetry(HWND owner) {
    for (int attempt = 0; attempt < 40; ++attempt) {
        if (OpenClipboard(owner)) return true;
        Sleep(attempt < 10 ? 40 : 100);
    }
    return false;
}

}  // namespace

bool CopyImageToClipboard(HWND owner, const BgraImage& image, std::wstring* error) {
    if (error) error->clear();
    if (!image.IsValid()) return Fail(L"The clipboard image is invalid.", error);

    std::vector<BYTE> png;
    if (!EncodePng(image, &png, error)) return false;
    HGLOBAL pngMemory = AllocateBytes(png.data(), png.size());
    if (!pngMemory) return Fail(L"Could not allocate PNG clipboard memory.", error);
    HGLOBAL dibMemory = CreateCompatibleDib(image);
    if (!dibMemory) {
        GlobalFree(pngMemory);
        return Fail(L"Could not allocate bitmap clipboard memory.", error);
    }

    bool pngTransferred = false;
    bool dibTransferred = false;
    if (!OpenClipboardWithRetry(owner)) {
        GlobalFree(pngMemory);
        GlobalFree(dibMemory);
        return Fail(L"Could not open the clipboard.", error);
    }

    bool emptied = EmptyClipboard() != FALSE;
    UINT pngFormat = RegisterClipboardFormatW(L"PNG");
    if (emptied && pngFormat != 0 && SetClipboardData(pngFormat, pngMemory)) {
        pngTransferred = true;
        if (SetClipboardData(CF_DIB, dibMemory)) {
            dibTransferred = true;
        }
    }
    CloseClipboard();

    if (!pngTransferred) GlobalFree(pngMemory);
    if (!dibTransferred) GlobalFree(dibMemory);
    if (!emptied) return Fail(L"Could not clear the clipboard.", error);
    if (!pngTransferred) return Fail(L"Could not publish PNG clipboard data.", error);
    if (!dibTransferred) return Fail(L"Could not publish bitmap clipboard data.", error);
    return true;
}

}  // namespace editor
