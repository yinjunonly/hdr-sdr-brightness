#include "../native_editor/dib_surface.h"

#include <windows.h>

#include <cstdio>

int main() {
    editor::DibSurface surface;
    if (!surface.Ensure(320, 180) || !surface.Bits()) {
        std::fprintf(stderr, "FAIL: persistent DIB surface was not created.\n");
        return 1;
    }
    void* originalBits = surface.Bits();
    HDC originalDc = surface.Dc();
    if (!surface.Ensure(320, 180) || surface.Bits() != originalBits ||
        surface.Dc() != originalDc) {
        std::fprintf(stderr, "FAIL: same-size paint recreated the DIB surface.\n");
        return 1;
    }

    editor::BgraImage image;
    image.width = 320;
    image.height = 180;
    image.pixels.assign(static_cast<size_t>(image.width) * image.height * 4, 0);
    image.pixels[0] = 17;
    image.pixels[1] = 34;
    image.pixels[2] = 51;
    image.pixels[3] = 255;
    if (!surface.Load(image)) {
        std::fprintf(stderr, "FAIL: BGRA image did not load into the DIB surface.\n");
        return 1;
    }
    const BYTE* pixels = static_cast<const BYTE*>(surface.Bits());
    if (pixels[0] != 17 || pixels[1] != 34 || pixels[2] != 51 || pixels[3] != 255) {
        std::fprintf(stderr, "FAIL: DIB surface changed top-down BGRA pixels.\n");
        return 1;
    }
    std::printf("PASS: native editor reuses a top-down DIB paint surface.\n");
    return 0;
}
