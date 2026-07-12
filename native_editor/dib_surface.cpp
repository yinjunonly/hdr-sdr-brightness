#include "dib_surface.h"

#include <cstring>

namespace editor {

DibSurface::~DibSurface() {
    Reset();
}

void DibSurface::ReleaseBitmap() {
    if (!bitmap_) return;
    if (dc_ && defaultBitmap_) SelectObject(dc_, defaultBitmap_);
    DeleteObject(bitmap_);
    bitmap_ = nullptr;
    bits_ = nullptr;
    width_ = 0;
    height_ = 0;
}

bool DibSurface::Ensure(int width, int height) {
    if (width <= 0 || height <= 0) return false;
    if (bitmap_ && width_ == width && height_ == height) return true;
    if (!dc_) {
        dc_ = CreateCompatibleDC(nullptr);
        if (!dc_) return false;
    }
    ReleaseBitmap();

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc_, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        return false;
    }
    HGDIOBJ previous = SelectObject(dc_, bitmap);
    if (!previous || previous == HGDI_ERROR) {
        DeleteObject(bitmap);
        return false;
    }
    if (!defaultBitmap_) defaultBitmap_ = previous;
    bitmap_ = bitmap;
    bits_ = bits;
    width_ = width;
    height_ = height;
    return true;
}

bool DibSurface::Load(const BgraImage& image) {
    if (!image.IsValid() || !Ensure(static_cast<int>(image.width),
                                    static_cast<int>(image.height))) {
        return false;
    }
    std::memcpy(bits_, image.pixels.data(), image.pixels.size());
    return true;
}

void DibSurface::Reset() {
    ReleaseBitmap();
    if (dc_) {
        DeleteDC(dc_);
        dc_ = nullptr;
    }
    defaultBitmap_ = nullptr;
}

}  // namespace editor
