#include "capture_options.h"

#include "native_common.h"

#include <shellapi.h>

#include <algorithm>
#include <cstdlib>
#include <cwchar>

namespace native_capture {

namespace {

bool TryParseLong(const wchar_t* text, LONG* value) {
    if (!text || !value) return false;
    wchar_t* end = nullptr;
    long parsed = std::wcstol(text, &end, 10);
    if (end == text || *end != L'\0') return false;
    *value = static_cast<LONG>(parsed);
    return true;
}

}  // namespace

Options ParseOptions() {
    Options options;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return options;

    for (int i = 1; i < argc; ++i) {
        if ((lstrcmpiW(argv[i], L"--output") == 0 || lstrcmpiW(argv[i], L"-o") == 0) && i + 1 < argc) {
            options.outputPath = argv[++i];
            options.saveOutput = true;
            options.outputSpecified = true;
        } else if (lstrcmpiW(argv[i], L"--sdr-white") == 0 && i + 1 < argc) {
            options.sdrWhite = std::clamp(static_cast<float>(_wtof(argv[++i])), 0.1f, 10.0f);
        } else if (lstrcmpiW(argv[i], L"--diagnostic") == 0) {
            options.diagnostic = true;
        } else if (lstrcmpiW(argv[i], L"--fullscreen-clip") == 0) {
            options.copyToClipboard = true;
            options.saveOutput = true;
        } else if (lstrcmpiW(argv[i], L"--select-region") == 0) {
            options.selectRegion = true;
        } else if (lstrcmpiW(argv[i], L"--lang") == 0 && i + 1 < argc) {
            LONG value = 0;
            if (TryParseLong(argv[++i], &value)) {
                options.language = static_cast<int>(value);
            }
        } else if (lstrcmpiW(argv[i], L"--parent-pid") == 0 && i + 1 < argc) {
            LONG value = 0;
            if (TryParseLong(argv[++i], &value) && value > 0) {
                options.parentPid = static_cast<DWORD>(value);
            }
        } else if (lstrcmpiW(argv[i], L"--open-folder") == 0) {
            // Accepted for command-line compatibility with the C# helper. The native prototype does not open Explorer.
        } else if (lstrcmpiW(argv[i], L"--clipboard") == 0) {
            options.copyToClipboard = true;
        } else if (lstrcmpiW(argv[i], L"--no-clipboard") == 0) {
            options.noClipboard = true;
        } else if (lstrcmpiW(argv[i], L"--no-output") == 0) {
            options.saveOutput = false;
        } else if (lstrcmpiW(argv[i], L"--server") == 0) {
            options.server = true;
        } else if (lstrcmpiW(argv[i], L"--server-once") == 0) {
            options.server = true;
            options.serverOnce = true;
        } else if (lstrcmpiW(argv[i], L"--pipe-name") == 0 && i + 1 < argc) {
            options.pipeName = argv[++i];
        } else if (lstrcmpiW(argv[i], L"--idle-timeout-ms") == 0 && i + 1 < argc) {
            LONG value = 0;
            if (TryParseLong(argv[++i], &value)) {
                options.idleTimeoutMs = static_cast<int>(value);
            }
        } else if (lstrcmpiW(argv[i], L"--region") == 0 && i + 4 < argc) {
            LONG x = 0;
            LONG y = 0;
            LONG width = 0;
            LONG height = 0;
            if (TryParseLong(argv[i + 1], &x) &&
                TryParseLong(argv[i + 2], &y) &&
                TryParseLong(argv[i + 3], &width) &&
                TryParseLong(argv[i + 4], &height)) {
                options.hasRegion = width > 0 && height > 0;
                options.regionX = x;
                options.regionY = y;
                options.regionWidth = width;
                options.regionHeight = height;
            }
            i += 4;
        }
    }

    LocalFree(argv);
    return options;
}

}  // namespace native_capture
