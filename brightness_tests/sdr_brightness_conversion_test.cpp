#include <cstdio>

#include "display_brightness.h"

namespace {

bool ExpectEqual(int actual, int expected, const char* message) {
    if (actual == expected) return true;
    std::fprintf(stderr, "FAIL: %s (expected %d, got %d)\n", message, expected, actual);
    return false;
}

}  // namespace

int main() {
    if (!ExpectEqual(SdrWhiteLevelToBrightnessPercent(1000), 0,
                     "the minimum SDR white level should map to 0 percent")) {
        return 1;
    }
    if (!ExpectEqual(SdrWhiteLevelToBrightnessPercent(2500), 30,
                     "the Windows level for 30 percent should round-trip")) {
        return 1;
    }
    if (!ExpectEqual(SdrWhiteLevelToBrightnessPercent(6000), 100,
                     "the maximum SDR white level should map to 100 percent")) {
        return 1;
    }

    std::puts("PASS: current SDR white level conversion.");
    return 0;
}
