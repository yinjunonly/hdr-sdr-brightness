#include <cstdio>

#include "brightness_initialization.h"

namespace {

bool Expect(bool condition, const char* message) {
    if (condition) return true;
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}

}  // namespace

int main() {
    {
        brightness_initialization::StoredState stored = {};

        brightness_initialization::Resolution result =
            brightness_initialization::Resolve(stored, true, 63);

        if (!Expect(result.ready,
                    "a fresh profile should be ready when current SDR brightness is readable")) {
            return 1;
        }
        if (!Expect(result.dayBrightness == 63 && result.nightBrightness == 63,
                    "fresh day and night values should both use the current SDR brightness")) {
            return 1;
        }
        if (!Expect(result.writeDay && result.writeNight && result.writeMarker,
                    "fresh initialization should persist both values and its completion marker")) {
            return 1;
        }
    }

    {
        brightness_initialization::StoredState stored = {};
        stored.hasDay = true;
        stored.dayBrightness = 55;
        stored.hasNight = true;
        stored.nightBrightness = 20;

        brightness_initialization::Resolution result =
            brightness_initialization::Resolve(stored, true, 80);

        if (!Expect(result.ready, "an existing profile should remain ready without reinitializing")) {
            return 1;
        }
        if (!Expect(result.dayBrightness == 55 && result.nightBrightness == 20,
                    "existing day and night values must not be replaced by the current brightness")) {
            return 1;
        }
        if (!Expect(!result.writeDay && !result.writeNight && result.writeMarker,
                    "an existing profile should only receive the one-time migration marker")) {
            return 1;
        }
    }

    {
        brightness_initialization::StoredState stored = {};
        stored.hasDay = true;
        stored.dayBrightness = 55;

        brightness_initialization::Resolution result =
            brightness_initialization::Resolve(stored, true, 80);

        if (!Expect(result.ready, "a partial profile should become ready when current brightness is readable")) {
            return 1;
        }
        if (!Expect(result.dayBrightness == 55 && result.nightBrightness == 80,
                    "a partial profile must preserve its stored value and only fill the missing value")) {
            return 1;
        }
        if (!Expect(!result.writeDay && result.writeNight && result.writeMarker,
                    "a partial profile should persist only the missing value and completion marker")) {
            return 1;
        }
    }

    {
        brightness_initialization::StoredState stored = {};

        brightness_initialization::Resolution result =
            brightness_initialization::Resolve(stored, false, 0);

        if (!Expect(!result.ready,
                    "a fresh profile must wait when current SDR brightness cannot be read")) {
            return 1;
        }
        if (!Expect(!result.writeDay && !result.writeNight && !result.writeMarker,
                    "a failed current-brightness read must not persist fallback defaults")) {
            return 1;
        }
    }

    {
        brightness_initialization::StoredState stored = {};
        stored.markerSet = true;

        brightness_initialization::Resolution result =
            brightness_initialization::Resolve(stored, true, 80);

        if (!Expect(!result.ready && !result.writeDay && !result.writeNight && !result.writeMarker,
                    "a completed marker must prevent recapturing current brightness over missing data")) {
            return 1;
        }
    }

    std::puts("PASS: first-run brightness initialization policy.");
    return 0;
}
