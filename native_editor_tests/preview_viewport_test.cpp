#include "../native_editor/preview_viewport.h"

#include <windows.h>

#include <cmath>
#include <cstdio>

namespace {

bool Expect(bool condition, const char* message) {
    if (condition) return true;
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}
bool Near(LONG first, LONG second, LONG tolerance = 2) {
    return std::labs(first - second) <= tolerance;
}

}  // namespace

int main() {
    editor::PreviewViewport viewport;
    viewport.SetImage(1600, 900);
    viewport.SetAvailable(RECT{100, 50, 1100, 650});

    RECT fit = viewport.Bounds();
    if (!Expect(fit.left == 100 && fit.right == 1100 &&
                    fit.top >= 68 && fit.top <= 69 && fit.bottom - fit.top == 563,
                "fit layout must center the full image inside the available area.")) {
        return 1;
    }

    POINT anchor{300, 250};
    POINT before = viewport.ClientToImage(anchor);
    if (!Expect(viewport.ZoomAt(anchor, WHEEL_DELTA),
                "one wheel notch must increase preview zoom.")) {
        return 1;
    }
    POINT after = viewport.ClientToImage(anchor);
    if (!Expect(Near(before.x, after.x) && Near(before.y, after.y),
                "wheel zoom must keep the image pixel under the pointer anchored.")) {
        return 1;
    }
    if (!Expect(viewport.Zoom() > 1.0 && viewport.Zoom() < 1.3,
                "one wheel notch must use a modest zoom step.")) {
        return 1;
    }

    RECT beforePan = viewport.Bounds();
    if (!Expect(viewport.PanBy(80, 0),
                "a zoomed preview must accept middle-button pan deltas.")) {
        return 1;
    }
    RECT afterPan = viewport.Bounds();
    if (!Expect(afterPan.left > beforePan.left,
                "dragging right must move the zoomed image right.")) {
        return 1;
    }

    for (int index = 0; index < 40; ++index) {
        viewport.ZoomAt(POINT{600, 350}, WHEEL_DELTA);
    }
    if (!Expect(std::fabs(viewport.Zoom() - 8.0) < 0.0001,
                "preview zoom must stop at 8x.")) {
        return 1;
    }
    viewport.PanBy(100000, 100000);
    RECT clamped = viewport.Bounds();
    if (!Expect(clamped.left <= 100 && clamped.top <= 50 &&
                    clamped.right >= 1100 && clamped.bottom >= 650,
                "panning must not expose blank space around a zoomed image.")) {
        return 1;
    }

    POINT imagePoint{1234, 678};
    POINT clientPoint = viewport.ImageToClient(imagePoint);
    POINT roundTrip = viewport.ClientToImage(clientPoint);
    if (!Expect(Near(imagePoint.x, roundTrip.x) && Near(imagePoint.y, roundTrip.y),
                "client/image coordinate conversion must remain physically aligned.")) {
        return 1;
    }

    for (int index = 0; index < 80; ++index) {
        viewport.ZoomAt(POINT{600, 350}, -WHEEL_DELTA);
    }
    RECT reset = viewport.Bounds();
    if (!Expect(std::fabs(viewport.Zoom() - 1.0) < 0.0001 &&
                    reset.left == fit.left && reset.top == fit.top &&
                    reset.right == fit.right && reset.bottom == fit.bottom,
                "zooming out must stop at the centered fit-to-window layout.")) {
        return 1;
    }

    std::puts("PASS: preview viewport zoom and pan preserve physical image coordinates.");
    return 0;
}
