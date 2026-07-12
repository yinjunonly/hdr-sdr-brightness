#include "fullscreen_capture_adapter.h"

#include "process_util.h"

#include <windows.h>

namespace fullscreen_capture {

namespace {

const wchar_t kNativePipeName[] = L"HdrSdrBrightnessNativeCapture";

std::wstring BuildCaptureCommand(const wchar_t* mode,
                                 const std::wstring& outputPath,
                                 int language,
                                 const std::wstring& sdrWhiteArgument) {
    std::wstring helperPath = GetNativeHelperPath();
    if (!FileExists(helperPath)) return L"";
    return QuotePath(helperPath) + mode +
        L" --output " + QuoteCommandLineArgument(outputPath) +
        L" --lang " + std::to_wstring(language) +
        sdrWhiteArgument;
}

}  // namespace

std::wstring GetNativeHelperPath() {
    return JoinPath(JoinPath(DirectoryFromPath(GetExePath()), L"capture"),
                    L"HdrSdrNativeCapture.exe");
}

std::wstring GetEditorHelperPath() {
    return JoinPath(JoinPath(DirectoryFromPath(GetExePath()), L"capture"),
                    L"HdrSdrNativeEditor.exe");
}

std::wstring GetNativePipeName() {
    return kNativePipeName;
}

std::wstring BuildNativeServerCommand(int language) {
    std::wstring helperPath = GetNativeHelperPath();
    if (!FileExists(helperPath)) return L"";
    return QuotePath(helperPath) +
        L" --server --pipe-name " + GetNativePipeName() +
        L" --parent-pid " + std::to_wstring(GetCurrentProcessId()) +
        L" --idle-timeout-ms 0" +
        L" --lang " + std::to_wstring(language);
}

std::wstring BuildEditorWarmupCommand() {
    std::wstring helperPath = GetEditorHelperPath();
    return FileExists(helperPath) ? QuotePath(helperPath) + L" --warmup" : L"";
}

std::wstring BuildFullscreenPipeCommand(int language,
                                        const std::wstring& outputPath,
                                        const std::wstring& sdrWhiteLevel) {
    std::wstring command = L"fullscreen-clip\t" + std::to_wstring(language) + L"\t" + outputPath;
    if (!sdrWhiteLevel.empty()) command += L"\t" + sdrWhiteLevel;
    return command;
}

std::wstring BuildCaptureFilePipeCommand(int language,
                                         const std::wstring& outputPath,
                                         const std::wstring& sdrWhiteLevel) {
    std::wstring command = L"capture-file\t" + std::to_wstring(language) + L"\t" + outputPath;
    if (!sdrWhiteLevel.empty()) command += L"\t" + sdrWhiteLevel;
    return command;
}

std::wstring BuildCaptureFileCommand(const std::wstring& outputPath,
                                     int language,
                                     const std::wstring& sdrWhiteArgument) {
    return BuildCaptureCommand(L" --no-clipboard", outputPath, language, sdrWhiteArgument);
}

std::wstring BuildEditorSelectCommand(const std::wstring& imagePath,
                                      const std::wstring& defaultOutputPath,
                                      int language) {
    std::wstring helperPath = GetEditorHelperPath();
    if (!FileExists(helperPath)) return L"";
    return QuotePath(helperPath) +
        L" --select-file " + QuoteCommandLineArgument(imagePath) +
        L" --output " + QuoteCommandLineArgument(defaultOutputPath) +
        L" --lang " + std::to_wstring(language);
}

std::wstring BuildEditorEditCommand(const std::wstring& imagePath,
                                    const std::wstring& defaultOutputPath,
                                    int language,
                                    bool skipInitialCopy) {
    std::wstring helperPath = GetEditorHelperPath();
    if (!FileExists(helperPath)) return L"";
    return QuotePath(helperPath) +
        L" --edit-file " + QuoteCommandLineArgument(imagePath) +
        L" --output " + QuoteCommandLineArgument(defaultOutputPath) +
        (skipInitialCopy ? L" --skip-initial-copy" : L"") +
        L" --lang " + std::to_wstring(language);
}

std::wstring BuildFullscreenCommand(const std::wstring& outputPath,
                                    int language,
                                    const std::wstring& sdrWhiteArgument) {
    return BuildCaptureCommand(L" --fullscreen-clip", outputPath, language, sdrWhiteArgument);
}

}  // namespace fullscreen_capture
