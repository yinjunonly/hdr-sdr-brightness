#pragma once

#include <string>

namespace fullscreen_capture {

std::wstring GetNativeHelperPath();
std::wstring GetEditorHelperPath();
std::wstring GetNativePipeName();

std::wstring BuildNativeServerCommand(int language);
std::wstring BuildEditorWarmupCommand();
std::wstring BuildFullscreenPipeCommand(int language,
                                        const std::wstring& outputPath,
                                        const std::wstring& sdrWhiteLevel);
std::wstring BuildCaptureFilePipeCommand(int language,
                                         const std::wstring& outputPath,
                                         const std::wstring& sdrWhiteLevel);
std::wstring BuildCaptureFileCommand(const std::wstring& outputPath,
                                     int language,
                                     const std::wstring& sdrWhiteArgument);
std::wstring BuildEditorSelectCommand(const std::wstring& imagePath,
                                      const std::wstring& defaultOutputPath,
                                      int language);
std::wstring BuildEditorEditCommand(const std::wstring& imagePath,
                                    const std::wstring& defaultOutputPath,
                                    int language,
                                    bool skipInitialCopy);
std::wstring BuildFullscreenCommand(const std::wstring& outputPath,
                                    int language,
                                    const std::wstring& sdrWhiteArgument);

}  // namespace fullscreen_capture
