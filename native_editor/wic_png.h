#pragma once

#include "image_document.h"

#include <string>
#include <vector>

namespace editor {

bool EncodePng(const BgraImage& image, std::vector<BYTE>* png, std::wstring* error);
bool SavePng(const std::wstring& path, const BgraImage& image, std::wstring* error);

}  // namespace editor
