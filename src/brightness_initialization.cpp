#include <algorithm>

#include "brightness_initialization.h"

namespace brightness_initialization {

Resolution Resolve(const StoredState& stored, bool hasCurrentBrightness, int currentBrightness) {
    Resolution result = {};
    if (stored.hasDay && stored.hasNight) {
        result.ready = true;
        result.dayBrightness = stored.dayBrightness;
        result.nightBrightness = stored.nightBrightness;
        result.writeMarker = !stored.markerSet;
        return result;
    }

    if (stored.markerSet || !hasCurrentBrightness) {
        return result;
    }

    int brightness = std::max(0, std::min(100, currentBrightness));
    result.ready = true;
    result.dayBrightness = stored.hasDay ? stored.dayBrightness : brightness;
    result.nightBrightness = stored.hasNight ? stored.nightBrightness : brightness;
    result.writeDay = !stored.hasDay;
    result.writeNight = !stored.hasNight;
    result.writeMarker = true;
    return result;
}

}  // namespace brightness_initialization
