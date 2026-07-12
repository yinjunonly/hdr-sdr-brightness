#include "../native_capture/capture_bitmap.h"

#include <cstdio>

int main() {
    native_capture::ToneMappedBitmap source;
    source.width = 3;
    source.height = 2;
    source.sourceFormat = 10;
    source.bgra = {
        1, 2, 3, 255,  4, 5, 6, 255,  7, 8, 9, 255,
        10, 11, 12, 255,  13, 14, 15, 255,  16, 17, 18, 255
    };

    RECT region{1, 0, 3, 2};
    native_capture::ToneMappedBitmap cropped = native_capture::CropToneMappedBitmap(source, region);
    const std::vector<BYTE> expected = {
        4, 5, 6, 255,  7, 8, 9, 255,
        13, 14, 15, 255,  16, 17, 18, 255
    };
    if (cropped.width != 2 || cropped.height != 2 || cropped.sourceFormat != source.sourceFormat ||
        cropped.bgra != expected) {
        std::fprintf(stderr, "FAIL: crop did not preserve the selected BGRA pixels.\n");
        return 1;
    }

    RECT invalid{2, 1, 2, 2};
    if (!native_capture::CropToneMappedBitmap(source, invalid).bgra.empty()) {
        std::fprintf(stderr, "FAIL: empty crop should return no pixels.\n");
        return 1;
    }

    std::printf("PASS: tone-mapped bitmap crop reuses the selected BGRA pixels.\n");
    return 0;
}
