#include "image_document.h"

#include "annotation_renderer.h"
#include "mosaic_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace editor {

namespace {

BYTE ToByte(float value) {
    int rounded = static_cast<int>(value * 255.0f + 0.5f);
    return static_cast<BYTE>(std::clamp(rounded, 0, 255));
}

void ApplyAdjustment(BgraImage* image, AdjustmentPreset preset) {
    if (!image || preset == AdjustmentPreset::Balanced) return;

    float exposure = preset == AdjustmentPreset::Low ? 0.88f : 1.12f;
    float highlightProtect = preset == AdjustmentPreset::Low ? 1.22f : 0.82f;
    for (size_t index = 0; index + 3 < image->pixels.size(); index += 4) {
        float b = image->pixels[index] / 255.0f * exposure;
        float g = image->pixels[index + 1] / 255.0f * exposure;
        float r = image->pixels[index + 2] / 255.0f * exposure;
        float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        if (luminance > 0.72f) {
            float over = luminance - 0.72f;
            float mapped = 0.72f + over / (1.0f + over * 4.0f * highlightProtect);
            float scale = mapped / std::max(0.0001f, luminance);
            r *= scale;
            g *= scale;
            b *= scale;
        }
        image->pixels[index] = ToByte(b);
        image->pixels[index + 1] = ToByte(g);
        image->pixels[index + 2] = ToByte(r);
        image->pixels[index + 3] = 255;
    }
}

}  // namespace

bool BgraImage::IsValid() const {
    if (width == 0 || height == 0) return false;
    uint64_t expected = static_cast<uint64_t>(width) * height * 4;
    return expected <= SIZE_MAX && pixels.size() == static_cast<size_t>(expected);
}

POINT PathBreakPoint() {
    return POINT{LONG_MIN, LONG_MIN};
}

bool IsPathBreak(POINT point) {
    return point.x == LONG_MIN && point.y == LONG_MIN;
}

bool CropImage(const BgraImage& source, const RECT& requested, BgraImage* output) {
    if (!output || !source.IsValid()) return false;
    RECT region{};
    region.left = std::clamp<LONG>(requested.left, 0, static_cast<LONG>(source.width));
    region.top = std::clamp<LONG>(requested.top, 0, static_cast<LONG>(source.height));
    region.right = std::clamp<LONG>(requested.right, region.left, static_cast<LONG>(source.width));
    region.bottom = std::clamp<LONG>(requested.bottom, region.top, static_cast<LONG>(source.height));
    if (region.right <= region.left || region.bottom <= region.top) return false;

    output->width = static_cast<UINT>(region.right - region.left);
    output->height = static_cast<UINT>(region.bottom - region.top);
    output->pixels.resize(static_cast<size_t>(output->width) * output->height * 4);
    size_t rowBytes = static_cast<size_t>(output->width) * 4;
    for (UINT y = 0; y < output->height; ++y) {
        size_t sourceOffset = (static_cast<size_t>(region.top + y) * source.width + region.left) * 4;
        std::copy_n(source.pixels.data() + sourceOffset,
                    rowBytes,
                    output->pixels.data() + static_cast<size_t>(y) * rowBytes);
    }
    return true;
}

ImageDocument::ImageDocument(const BgraImage& source) : source_(source) {}

ImageDocument::ImageDocument(BgraImage&& source) : source_(std::move(source)) {}

void ImageDocument::AddOperation(const EditOperation& operation) {
    operations_.push_back(operation);
    redoOperations_.clear();
}

bool ImageDocument::Undo() {
    if (operations_.empty()) return false;
    redoOperations_.push_back(operations_.back());
    operations_.pop_back();
    return true;
}

bool ImageDocument::Redo() {
    if (redoOperations_.empty()) return false;
    operations_.push_back(redoOperations_.back());
    redoOperations_.pop_back();
    return true;
}

void ImageDocument::Reset() {
    operations_.clear();
    redoOperations_.clear();
    preset_ = AdjustmentPreset::Balanced;
}

BgraImage ImageDocument::Render() const {
    BgraImage output = source_;
    ApplyAdjustment(&output, preset_);
    RenderMosaics(&output, operations_);
    RenderVectorAnnotations(&output, operations_);
    return output;
}

}  // namespace editor
