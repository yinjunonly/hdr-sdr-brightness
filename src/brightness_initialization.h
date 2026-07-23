#pragma once

namespace brightness_initialization {

struct StoredState {
    bool markerSet;
    bool hasDay;
    int dayBrightness;
    bool hasNight;
    int nightBrightness;
};

struct Resolution {
    bool ready;
    int dayBrightness;
    int nightBrightness;
    bool writeDay;
    bool writeNight;
    bool writeMarker;
};

Resolution Resolve(const StoredState& stored, bool hasCurrentBrightness, int currentBrightness);

}  // namespace brightness_initialization
