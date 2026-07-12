#include "annotation_renderer.h"

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <vector>

namespace editor {

namespace {

Gdiplus::Color AnnotationColor(COLORREF color) {
    return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
}

void DrawPenPath(Gdiplus::Graphics* graphics,
                 Gdiplus::Pen* pen,
                 const std::vector<POINT>& points) {
    std::vector<Gdiplus::Point> segment;
    auto flush = [&]() {
        if (segment.size() > 1) {
            graphics->DrawLines(pen, segment.data(), static_cast<INT>(segment.size()));
        }
        segment.clear();
    };
    for (POINT point : points) {
        if (IsPathBreak(point)) {
            flush();
        } else {
            segment.emplace_back(point.x, point.y);
        }
    }
    flush();
}

}  // namespace

void RenderVectorAnnotations(BgraImage* image, const std::vector<EditOperation>& operations) {
    if (!image || !image->IsValid()) return;

    Gdiplus::Bitmap bitmap(static_cast<INT>(image->width),
                           static_cast<INT>(image->height),
                           static_cast<INT>(image->width * 4),
                           PixelFormat32bppARGB,
                           image->pixels.data());
    if (bitmap.GetLastStatus() != Gdiplus::Ok) return;

    Gdiplus::Graphics graphics(&bitmap);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    for (const EditOperation& operation : operations) {
        if (operation.type == EditOperationType::Mosaic) continue;

        Gdiplus::Pen pen(AnnotationColor(operation.color),
                         static_cast<Gdiplus::REAL>(std::max(1, operation.strokeWidth)));
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        int width = std::max(0L, operation.rect.right - operation.rect.left);
        int height = std::max(0L, operation.rect.bottom - operation.rect.top);
        if (operation.type == EditOperationType::Marker && width > 0 && height > 0) {
            graphics.DrawRectangle(&pen, operation.rect.left, operation.rect.top, width, height);
        } else if (operation.type == EditOperationType::Ellipse && width > 0 && height > 0) {
            graphics.DrawEllipse(&pen, operation.rect.left, operation.rect.top, width, height);
        } else if (operation.type == EditOperationType::Pen && operation.points.size() > 1) {
            DrawPenPath(&graphics, &pen, operation.points);
        }
    }
    graphics.Flush(Gdiplus::FlushIntentionSync);
    for (size_t index = 3; index < image->pixels.size(); index += 4) {
        image->pixels[index] = 255;
    }
}

}  // namespace editor
