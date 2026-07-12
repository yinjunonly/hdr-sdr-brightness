#pragma once

#include <windows.h>

#include <string>

namespace native_capture {

struct NativeCaptureRuntime;

struct CaptureRequest {
    std::wstring outputPath = L"native-capture.bmp";
    float sdrWhite = 3.5f;
    bool diagnostic = false;
    bool copyToClipboard = false;
    bool saveOutput = true;
    bool hasRegion = false;
    LONG regionX = 0;
    LONG regionY = 0;
    LONG regionWidth = 0;
    LONG regionHeight = 0;
};

int RunCaptureRequest(const CaptureRequest& request);
int RunCaptureRequest(const CaptureRequest& request, NativeCaptureRuntime* runtime);
int RunSelectedRegionCapture(CaptureRequest request);
int RunSelectedRegionCapture(CaptureRequest request, NativeCaptureRuntime* runtime);

}  // namespace native_capture
