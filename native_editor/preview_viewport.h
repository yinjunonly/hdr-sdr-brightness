#pragma once

#include <windows.h>

namespace editor {

class PreviewViewport {
public:
    void SetImage(UINT width, UINT height);
    void SetAvailable(const RECT& available);

    bool ZoomAt(POINT clientPoint, int wheelDelta);
    bool PanBy(int deltaX, int deltaY);

    RECT Bounds() const { return bounds_; }
    RECT Available() const { return available_; }
    double Zoom() const { return zoom_; }
    bool IsZoomed() const { return zoom_ > 1.0001; }
    POINT ClientToImage(POINT point) const;
    POINT ImageToClient(POINT point) const;

private:
    double FitScale() const;
    void Recalculate();

    UINT imageWidth_ = 0;
    UINT imageHeight_ = 0;
    RECT available_{};
    RECT bounds_{};
    double zoom_ = 1.0;
    double centerX_ = 0.0;
    double centerY_ = 0.0;
};

}  // namespace editor
