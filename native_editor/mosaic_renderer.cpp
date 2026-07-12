#include "mosaic_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace editor {

namespace {

RECT ClipRect(const RECT& value, UINT width, UINT height) {
    RECT result{};
    result.left = std::clamp<LONG>(value.left, 0, static_cast<LONG>(width));
    result.top = std::clamp<LONG>(value.top, 0, static_cast<LONG>(height));
    result.right = std::clamp<LONG>(value.right, result.left, static_cast<LONG>(width));
    result.bottom = std::clamp<LONG>(value.bottom, result.top, static_cast<LONG>(height));
    return result;
}

BYTE* Pixel(BgraImage* image, int x, int y) {
    return image->pixels.data() + (static_cast<size_t>(y) * image->width + x) * 4;
}

const BYTE* Pixel(const BgraImage& image, int x, int y) {
    return image.pixels.data() + (static_cast<size_t>(y) * image.width + x) * 4;
}

void AverageCell(const BgraImage& source, const RECT& requested, BYTE color[4]) {
    RECT rect = ClipRect(requested, source.width, source.height);
    uint64_t sums[3]{};
    uint64_t count = 0;
    for (int y = rect.top; y < rect.bottom; ++y) {
        for (int x = rect.left; x < rect.right; ++x) {
            const BYTE* pixel = Pixel(source, x, y);
            sums[0] += pixel[0];
            sums[1] += pixel[1];
            sums[2] += pixel[2];
            ++count;
        }
    }
    if (count == 0) {
        std::memset(color, 0, 4);
        color[3] = 255;
        return;
    }
    color[0] = static_cast<BYTE>((sums[0] + count / 2) / count);
    color[1] = static_cast<BYTE>((sums[1] + count / 2) / count);
    color[2] = static_cast<BYTE>((sums[2] + count / 2) / count);
    color[3] = 255;
}

void ApplyMosaicRect(BgraImage* image,
                     const BgraImage& source,
                     RECT requested,
                     int preferredBlockSize) {
    RECT rect = ClipRect(requested, image->width, image->height);
    if (rect.right <= rect.left || rect.bottom <= rect.top) return;
    int block = std::max(8, preferredBlockSize);
    for (int y = rect.top; y < rect.bottom; y += block) {
        for (int x = rect.left; x < rect.right; x += block) {
            int right = std::min<int>(rect.right, x + block);
            int bottom = std::min<int>(rect.bottom, y + block);
            BYTE color[4]{};
            AverageCell(source, RECT{x, y, right, bottom}, color);
            for (int py = y; py < bottom; ++py) {
                for (int px = x; px < right; ++px) {
                    std::memcpy(Pixel(image, px, py), color, 4);
                }
            }
        }
    }
}

RECT BrushBounds(const EditOperation& operation, UINT width, UINT height) {
    int radius = std::max(4, operation.strokeWidth / 2);
    RECT bounds{static_cast<LONG>(width), static_cast<LONG>(height), 0, 0};
    bool hasPoint = false;
    for (POINT point : operation.points) {
        if (IsPathBreak(point)) continue;
        hasPoint = true;
        bounds.left = std::min(bounds.left, point.x - radius);
        bounds.top = std::min(bounds.top, point.y - radius);
        bounds.right = std::max(bounds.right, point.x + radius + 1);
        bounds.bottom = std::max(bounds.bottom, point.y + radius + 1);
    }
    return hasPoint ? ClipRect(bounds, width, height) : RECT{};
}

void StampCircle(std::vector<BYTE>* mask,
                 int maskWidth,
                 const RECT& bounds,
                 int centerX,
                 int centerY,
                 int radius) {
    int left = std::max<int>(bounds.left, centerX - radius);
    int top = std::max<int>(bounds.top, centerY - radius);
    int right = std::min<int>(bounds.right, centerX + radius + 1);
    int bottom = std::min<int>(bounds.bottom, centerY + radius + 1);
    int radiusSquared = radius * radius;
    for (int y = top; y < bottom; ++y) {
        int dy = y - centerY;
        for (int x = left; x < right; ++x) {
            int dx = x - centerX;
            if (dx * dx + dy * dy <= radiusSquared) {
                (*mask)[static_cast<size_t>(y - bounds.top) * maskWidth + (x - bounds.left)] = 1;
            }
        }
    }
}

void ApplyMosaicBrush(BgraImage* image,
                      const BgraImage& source,
                      const EditOperation& operation) {
    RECT bounds = BrushBounds(operation, image->width, image->height);
    int maskWidth = bounds.right - bounds.left;
    int maskHeight = bounds.bottom - bounds.top;
    if (maskWidth <= 0 || maskHeight <= 0) return;

    int radius = std::max(4, operation.strokeWidth / 2);
    int stepLimit = std::max(1, radius / 3);
    std::vector<BYTE> mask(static_cast<size_t>(maskWidth) * maskHeight, 0);
    POINT previous{};
    bool hasPrevious = false;
    for (POINT point : operation.points) {
        if (IsPathBreak(point)) {
            hasPrevious = false;
            continue;
        }
        int steps = 1;
        if (hasPrevious) {
            double dx = static_cast<double>(point.x - previous.x);
            double dy = static_cast<double>(point.y - previous.y);
            steps = std::max(1, static_cast<int>(std::ceil(std::sqrt(dx * dx + dy * dy) / stepLimit)));
        }
        for (int step = hasPrevious ? 1 : 0; step <= steps; ++step) {
            double amount = steps == 0 ? 1.0 : step / static_cast<double>(steps);
            int x = hasPrevious
                ? previous.x + static_cast<int>((point.x - previous.x) * amount + 0.5)
                : point.x;
            int y = hasPrevious
                ? previous.y + static_cast<int>((point.y - previous.y) * amount + 0.5)
                : point.y;
            StampCircle(&mask, maskWidth, bounds, x, y, radius);
        }
        previous = point;
        hasPrevious = true;
    }

    int cell = MosaicBrushCellSize(operation.strokeWidth);
    int firstCellX = bounds.left / cell * cell;
    int firstCellY = bounds.top / cell * cell;
    for (int y = firstCellY; y < bounds.bottom; y += cell) {
        for (int x = firstCellX; x < bounds.right; x += cell) {
            RECT gridCell{x, y, x + cell, y + cell};
            RECT clipped = ClipRect(gridCell, image->width, image->height);
            BYTE color[4]{};
            AverageCell(source, clipped, color);
            int writeTop = std::max(clipped.top, bounds.top);
            int writeBottom = std::min(clipped.bottom, bounds.bottom);
            int writeLeft = std::max(clipped.left, bounds.left);
            int writeRight = std::min(clipped.right, bounds.right);
            for (int py = writeTop; py < writeBottom; ++py) {
                for (int px = writeLeft; px < writeRight; ++px) {
                    size_t maskIndex = static_cast<size_t>(py - bounds.top) * maskWidth +
                        (px - bounds.left);
                    if (mask[maskIndex]) std::memcpy(Pixel(image, px, py), color, 4);
                }
            }
        }
    }
}

}  // namespace

int MosaicBrushCellSize(int brushSize) {
    return std::max(4, brushSize / 3);
}

COLORREF MosaicCellAverage(const BgraImage& image, const RECT& requested) {
    if (!image.IsValid()) return RGB(0, 0, 0);
    BYTE color[4]{};
    AverageCell(image, requested, color);
    return RGB(color[2], color[1], color[0]);
}

void RenderMosaics(BgraImage* image, const std::vector<EditOperation>& operations) {
    if (!image || !image->IsValid()) return;
    for (const EditOperation& operation : operations) {
        if (operation.type != EditOperationType::Mosaic) continue;
        BgraImage source = *image;
        if (!operation.points.empty()) {
            ApplyMosaicBrush(image, source, operation);
        } else {
            ApplyMosaicRect(image, source, operation.rect, operation.strokeWidth);
        }
    }
}

}  // namespace editor
