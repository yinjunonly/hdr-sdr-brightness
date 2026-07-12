#pragma once

#include "image_document.h"

#include <vector>

namespace editor {

int MosaicBrushCellSize(int brushSize);
COLORREF MosaicCellAverage(const BgraImage& image, const RECT& requested);
void RenderMosaics(BgraImage* image, const std::vector<EditOperation>& operations);

}  // namespace editor
