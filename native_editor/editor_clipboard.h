#pragma once

#include "image_document.h"

#include <windows.h>

#include <string>

namespace editor {

bool CopyImageToClipboard(HWND owner, const BgraImage& image, std::wstring* error);

}  // namespace editor
