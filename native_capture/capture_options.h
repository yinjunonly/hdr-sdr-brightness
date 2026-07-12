#pragma once

#include "capture_pipeline.h"

namespace native_capture {

struct Options : CaptureRequest {
    bool server = false;
    bool serverOnce = false;
    bool selectRegion = false;
    bool outputSpecified = false;
    bool noClipboard = false;
    std::wstring pipeName = L"HdrSdrBrightnessCapture";
    int idleTimeoutMs = 90000;
    int language = 0;
    DWORD parentPid = 0;
};

Options ParseOptions();

}  // namespace native_capture
