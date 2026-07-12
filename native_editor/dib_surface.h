#pragma once

#include "image_document.h"

#include <windows.h>

namespace editor {

class DibSurface {
public:
    DibSurface() = default;
    ~DibSurface();

    DibSurface(const DibSurface&) = delete;
    DibSurface& operator=(const DibSurface&) = delete;

    bool Ensure(int width, int height);
    bool Load(const BgraImage& image);
    void Reset();

    HDC Dc() const { return dc_; }
    void* Bits() const { return bits_; }
    int Width() const { return width_; }
    int Height() const { return height_; }

private:
    void ReleaseBitmap();

    HDC dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ defaultBitmap_ = nullptr;
    void* bits_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace editor
