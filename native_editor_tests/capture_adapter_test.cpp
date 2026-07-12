#include "../src/fullscreen_capture_adapter.h"

#include <windows.h>

#include <cstdio>
#include <string>

namespace {

bool Contains(const std::wstring& value, const wchar_t* expected) {
    return value.find(expected) != std::wstring::npos;
}

}  // namespace

int main() {
    std::wstring editor = fullscreen_capture::GetEditorHelperPath();
    if (!Contains(editor, L"HdrSdrNativeEditor.exe") ||
        Contains(editor, L"HdrSdrEditor.exe")) {
        std::fprintf(stderr, "FAIL: main adapter still resolves the managed editor.\n");
        return 1;
    }
    std::wstring preferred = fullscreen_capture::BuildEditorEditCommand(
        L"C:\\Temp\\capture.bmp",
        L"C:\\Temp\\capture.png",
        2,
        true);
    if (!Contains(preferred, L"HdrSdrNativeEditor.exe") ||
        !Contains(preferred, L"--edit-file") ||
        !Contains(preferred, L"--skip-initial-copy")) {
        std::fprintf(stderr, "FAIL: fullscreen edit command does not launch the native editor.\n");
        return 1;
    }
    std::wstring select = fullscreen_capture::BuildEditorSelectCommand(
        L"C:\\Temp\\capture.bmp", L"C:\\Temp\\capture.png", 2);
    if (!Contains(select, L"HdrSdrNativeEditor.exe") || !Contains(select, L"--select-file")) {
        std::fprintf(stderr, "FAIL: region edit command does not launch native select mode.\n");
        return 1;
    }
    std::wstring warmup = fullscreen_capture::BuildEditorWarmupCommand();
    if (!Contains(warmup, L"HdrSdrNativeEditor.exe") ||
        !Contains(warmup, L"--warmup")) {
        std::fprintf(stderr, "FAIL: native editor warmup command is incorrect.\n");
        return 1;
    }
    std::wstring captureFile = fullscreen_capture::BuildCaptureFileCommand(
        L"C:\\Temp\\capture.bmp", 2, L" --sdr-white 3.500");
    std::wstring fullscreen = fullscreen_capture::BuildFullscreenCommand(
        L"C:\\Temp\\capture.bmp", 2, L" --sdr-white 3.500");
    if (!Contains(captureFile, L"HdrSdrNativeCapture.exe") ||
        !Contains(captureFile, L"--no-clipboard") ||
        !Contains(fullscreen, L"--fullscreen-clip")) {
        std::fprintf(stderr, "FAIL: native one-shot capture fallbacks are incomplete.\n");
        return 1;
    }
    std::printf("PASS: tray commands resolve the short-lived native C++ editor.\n");
    return 0;
}
