#pragma once

#include <windows.h>
#include <string>

namespace capture_pipe {

bool SendCommand(const std::wstring& command, DWORD timeoutMs);
bool SendCommandForExitCode(const std::wstring& command, DWORD timeoutMs, DWORD* exitCode);
bool SendCommandToPipe(const std::wstring& pipeName, const std::wstring& command, DWORD timeoutMs);
bool SendCommandForExitCodeToPipe(const std::wstring& pipeName,
                                  const std::wstring& command,
                                  DWORD timeoutMs,
                                  DWORD* exitCode);

}
