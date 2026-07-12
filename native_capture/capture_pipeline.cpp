#include "capture_pipeline.h"

#include "bmp_output.h"
#include "capture_bitmap.h"
#include "clipboard_output.h"
#include "native_capture_backend.h"
#include "native_common.h"
#include "selection_overlay.h"
#include "tone_map.h"

#include <cstdio>

namespace native_capture {

int RunCaptureRequest(const CaptureRequest& request) {
    return RunCaptureRequest(request, nullptr);
}

int RunCaptureRequest(const CaptureRequest& request, NativeCaptureRuntime* runtime) {
    NativeCapturedFrame frame;
    if (runtime) {
        if (!CaptureFrame(runtime, request.diagnostic, &frame)) return 8;
    } else {
        if (!CapturePrimaryMonitorFrame(request.diagnostic, &frame)) return 8;
    }

    ReadbackRegion region;
    region.enabled = request.hasRegion;
    region.x = request.regionX;
    region.y = request.regionY;
    region.width = request.regionWidth;
    region.height = request.regionHeight;

    ToneMappedBitmap bitmap = ReadbackAndToneMap(frame.device.Get(), frame.context.Get(), frame.texture.Get(),
                                                request.sdrWhite, request.diagnostic, region);
    if (bitmap.bgra.empty()) return 8;

    if (request.diagnostic) {
        bitmap.stats.Print(bitmap.sourceFormat);
    }

    if (request.saveOutput) {
        Stopwatch saveTimer;
        if (!SaveTopDownBmp(request.outputPath, bitmap.width, bitmap.height, bitmap.bgra)) return 8;
        if (request.diagnostic) {
            std::printf("Timing bmp.save: %.1f ms\n", saveTimer.ElapsedMs());
        }
        std::wprintf(L"Saved BMP: %ls\n", request.outputPath.c_str());
    }

    if (request.copyToClipboard) {
        Stopwatch clipboardTimer;
        if (!CopyBgraToClipboard(bitmap.width, bitmap.height, bitmap.bgra)) return 8;
        if (request.diagnostic) {
            std::printf("Timing clipboard.copy: %.1f ms\n", clipboardTimer.ElapsedMs());
        }
        std::printf(request.hasRegion
                        ? "Copied selected region to clipboard.\n"
                        : "Fullscreen screenshot copied to clipboard.\n");
    }

    return 0;
}

int RunSelectedRegionCapture(CaptureRequest request) {
    return RunSelectedRegionCapture(request, nullptr);
}

int RunSelectedRegionCapture(CaptureRequest request, NativeCaptureRuntime* runtime) {
    NativeCapturedFrame frame;
    if (runtime) {
        if (!CaptureFrame(runtime, request.diagnostic, &frame)) return 8;
    } else {
        if (!CapturePrimaryMonitorFrame(request.diagnostic, &frame)) return 8;
    }

    ToneMappedBitmap fullBitmap = ReadbackAndToneMap(frame.device.Get(), frame.context.Get(), frame.texture.Get(),
                                                    request.sdrWhite, request.diagnostic);
    if (fullBitmap.bgra.empty()) return 8;

    RECT region{};
    if (!SelectPrimaryMonitorRegion(fullBitmap.width, fullBitmap.height, fullBitmap.bgra.data(), &region)) {
        std::printf("Region selection cancelled.\n");
        return 2;
    }

    request.hasRegion = true;
    request.regionX = region.left;
    request.regionY = region.top;
    request.regionWidth = region.right - region.left;
    request.regionHeight = region.bottom - region.top;

    if (request.diagnostic) {
        std::printf("Selected region: %ld,%ld %ldx%ld\n",
                    request.regionX,
                    request.regionY,
                    request.regionWidth,
                    request.regionHeight);
    }

    ToneMappedBitmap bitmap = CropToneMappedBitmap(fullBitmap, region);
    if (bitmap.bgra.empty()) return 8;

    if (request.diagnostic) {
        std::printf("Reused tone-mapped preview pixels for selected output.\n");
    }
    if (request.saveOutput) {
        Stopwatch saveTimer;
        if (!SaveTopDownBmp(request.outputPath, bitmap.width, bitmap.height, bitmap.bgra)) return 8;
        if (request.diagnostic) {
            std::printf("Timing bmp.save: %.1f ms\n", saveTimer.ElapsedMs());
        }
        std::wprintf(L"Saved BMP: %ls\n", request.outputPath.c_str());
    }
    if (request.copyToClipboard) {
        Stopwatch clipboardTimer;
        if (!CopyBgraToClipboard(bitmap.width, bitmap.height, bitmap.bgra)) return 8;
        if (request.diagnostic) {
            std::printf("Timing clipboard.copy: %.1f ms\n", clipboardTimer.ElapsedMs());
        }
        std::printf("Copied selected region to clipboard.\n");
    }
    return 0;
}

}  // namespace native_capture
