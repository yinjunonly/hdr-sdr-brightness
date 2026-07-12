#pragma once

#include <windows.h>

namespace native_capture {

bool SelectPrimaryMonitorRegion(UINT previewWidth, UINT previewHeight, const BYTE* previewBgra, RECT* region);

}  // namespace native_capture
