#pragma once

#include "tone_map.h"

namespace native_capture {

ToneMappedBitmap CropToneMappedBitmap(const ToneMappedBitmap& source, const RECT& region);

}  // namespace native_capture
