#include "capture_options.h"
#include "capture_pipeline.h"
#include "native_pipe_server.h"

#include <roapi.h>

using namespace native_capture;

int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Options options = ParseOptions();
    std::printf("HDR SDR Native Capture\n");
    std::printf("Tone map: desktop, SDR white %.2f, SDR output white 1.00, HDR knee 0.55, HDR shoulder 5.00, exposure 0.75\n",
                options.sdrWhite);

    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::fprintf(stderr, "RoInitialize failed: 0x%08lx\n", static_cast<unsigned long>(hr));
        return 8;
    }

    if (options.server) {
        return RunPipeServer(options);
    }

    if (options.selectRegion) {
        options.copyToClipboard = !options.noClipboard;
        options.saveOutput = options.outputSpecified && options.saveOutput;
        return RunSelectedRegionCapture(options);
    }

    return RunCaptureRequest(options);
}
