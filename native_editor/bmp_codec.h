#pragma once

#include "image_document.h"

#include <string>

namespace editor {

bool LoadBmp(const std::wstring& path, BgraImage* image, std::wstring* error);

}  // namespace editor
