#pragma once

#include "image_document.h"

#include <vector>

namespace editor {

void RenderVectorAnnotations(BgraImage* image, const std::vector<EditOperation>& operations);

}  // namespace editor
