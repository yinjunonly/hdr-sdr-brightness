#include "preview_viewport.h"

#include <algorithm>
#include <cmath>

namespace editor {

namespace {

constexpr double kWheelZoomStep = 1.2;
constexpr double kMaximumZoom = 8.0;

int Width(const RECT& rect) {
    return std::max(0L, rect.right - rect.left);
}

int Height(const RECT& rect) {
    return std::max(0L, rect.bottom - rect.top);
}

}  // namespace

void PreviewViewport::SetImage(UINT width, UINT height) {
    if (width == imageWidth_ && height == imageHeight_) return;
    imageWidth_ = width;
    imageHeight_ = height;
    zoom_ = 1.0;
    centerX_ = width / 2.0;
    centerY_ = height / 2.0;
    Recalculate();
}

void PreviewViewport::SetAvailable(const RECT& available) {
    available_ = available;
    Recalculate();
}

double PreviewViewport::FitScale() const {
    if (imageWidth_ == 0 || imageHeight_ == 0 ||
        Width(available_) == 0 || Height(available_) == 0) {
        return 1.0;
    }
    return std::min(Width(available_) / static_cast<double>(imageWidth_),
                    Height(available_) / static_cast<double>(imageHeight_));
}

void PreviewViewport::Recalculate() {
    if (imageWidth_ == 0 || imageHeight_ == 0 ||
        Width(available_) == 0 || Height(available_) == 0) {
        bounds_ = RECT{};
        return;
    }

    double scale = FitScale() * zoom_;
    int scaledWidth = std::max(1, static_cast<int>(std::lround(imageWidth_ * scale)));
    int scaledHeight = std::max(1, static_cast<int>(std::lround(imageHeight_ * scale)));
    double availableCenterX = (available_.left + available_.right) / 2.0;
    double availableCenterY = (available_.top + available_.bottom) / 2.0;

    LONG left = 0;
    if (scaledWidth <= Width(available_)) {
        centerX_ = imageWidth_ / 2.0;
        left = available_.left + (Width(available_) - scaledWidth) / 2;
    } else {
        double halfVisible = Width(available_) / (2.0 * scale);
        centerX_ = std::clamp(centerX_, halfVisible, imageWidth_ - halfVisible);
        left = static_cast<LONG>(std::lround(availableCenterX - centerX_ * scale));
        left = std::clamp<LONG>(left, available_.right - scaledWidth, available_.left);
    }

    LONG top = 0;
    if (scaledHeight <= Height(available_)) {
        centerY_ = imageHeight_ / 2.0;
        top = available_.top + (Height(available_) - scaledHeight) / 2;
    } else {
        double halfVisible = Height(available_) / (2.0 * scale);
        centerY_ = std::clamp(centerY_, halfVisible, imageHeight_ - halfVisible);
        top = static_cast<LONG>(std::lround(availableCenterY - centerY_ * scale));
        top = std::clamp<LONG>(top, available_.bottom - scaledHeight, available_.top);
    }
    bounds_ = RECT{left, top, left + scaledWidth, top + scaledHeight};
}

bool PreviewViewport::ZoomAt(POINT clientPoint, int wheelDelta) {
    if (wheelDelta == 0 || imageWidth_ == 0 || imageHeight_ == 0 ||
        Width(available_) == 0 || Height(available_) == 0) {
        return false;
    }

    double factor = std::pow(kWheelZoomStep,
                             wheelDelta / static_cast<double>(WHEEL_DELTA));
    double nextZoom = std::clamp(zoom_ * factor, 1.0, kMaximumZoom);
    if (std::fabs(nextZoom - zoom_) < 0.000001) return false;

    double oldScaleX = Width(bounds_) / static_cast<double>(imageWidth_);
    double oldScaleY = Height(bounds_) / static_cast<double>(imageHeight_);
    double anchorImageX = (clientPoint.x - bounds_.left) / oldScaleX;
    double anchorImageY = (clientPoint.y - bounds_.top) / oldScaleY;

    zoom_ = nextZoom;
    double newScale = FitScale() * zoom_;
    double availableCenterX = (available_.left + available_.right) / 2.0;
    double availableCenterY = (available_.top + available_.bottom) / 2.0;
    centerX_ = anchorImageX - (clientPoint.x - availableCenterX) / newScale;
    centerY_ = anchorImageY - (clientPoint.y - availableCenterY) / newScale;
    Recalculate();
    return true;
}

bool PreviewViewport::PanBy(int deltaX, int deltaY) {
    if (!IsZoomed() || imageWidth_ == 0 || imageHeight_ == 0) return false;
    RECT before = bounds_;
    double scale = FitScale() * zoom_;
    centerX_ -= deltaX / scale;
    centerY_ -= deltaY / scale;
    Recalculate();
    return before.left != bounds_.left || before.top != bounds_.top;
}

POINT PreviewViewport::ClientToImage(POINT point) const {
    if (imageWidth_ == 0 || imageHeight_ == 0 || Width(bounds_) == 0 || Height(bounds_) == 0) {
        return POINT{};
    }
    int x = static_cast<int>(std::lround(
        (point.x - bounds_.left) * imageWidth_ / static_cast<double>(Width(bounds_))));
    int y = static_cast<int>(std::lround(
        (point.y - bounds_.top) * imageHeight_ / static_cast<double>(Height(bounds_))));
    return POINT{std::clamp<LONG>(x, 0, static_cast<LONG>(imageWidth_) - 1),
                 std::clamp<LONG>(y, 0, static_cast<LONG>(imageHeight_) - 1)};
}

POINT PreviewViewport::ImageToClient(POINT point) const {
    if (imageWidth_ == 0 || imageHeight_ == 0) return POINT{};
    double scaleX = Width(bounds_) / static_cast<double>(imageWidth_);
    double scaleY = Height(bounds_) / static_cast<double>(imageHeight_);
    return POINT{bounds_.left + static_cast<LONG>(std::lround(point.x * scaleX)),
                 bounds_.top + static_cast<LONG>(std::lround(point.y * scaleY))};
}

}  // namespace editor
