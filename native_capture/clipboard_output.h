#pragma once

#include <windows.h>

#include <vector>

namespace native_capture {

bool CopyBgraToClipboard(UINT width, UINT height, const std::vector<BYTE>& bgra);

}  // namespace native_capture
