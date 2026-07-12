#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace native_capture {

bool SaveTopDownBmp(const std::wstring& path, UINT width, UINT height, const std::vector<BYTE>& bgra);

}  // namespace native_capture
