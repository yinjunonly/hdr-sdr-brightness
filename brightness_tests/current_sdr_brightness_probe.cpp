#include <cstdio>

#include "display_brightness.h"

int main() {
    int brightness = -1;
    if (!ReadCurrentSdrBrightness(&brightness)) {
        std::fprintf(stderr, "FAIL: current SDR brightness was not readable on the active HDR display.\n");
        return 1;
    }
    if (brightness < 0 || brightness > 100) {
        std::fprintf(stderr, "FAIL: current SDR brightness was outside 0-100: %d\n", brightness);
        return 1;
    }

    std::printf("PASS: current SDR brightness is %d%%.\n", brightness);
    return 0;
}
