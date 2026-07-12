#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>

#include "capture_pipe.h"

namespace {

const wchar_t kCapturePipeName[] = L"\\\\.\\pipe\\HdrSdrBrightnessCapture";

std::wstring PipePath(const std::wstring& pipeName) {
    const std::wstring prefix = L"\\\\.\\pipe\\";
    if (pipeName.rfind(prefix, 0) == 0) return pipeName;
    return prefix + pipeName;
}

DWORD ElapsedSince(DWORD startTick) {
    return GetTickCount() - startTick;
}

void WaitForNextPipeAttempt(DWORD error, DWORD remainingMs) {
    if (error == ERROR_PIPE_BUSY) {
        WaitNamedPipeW(kCapturePipeName, std::min<DWORD>(remainingMs, 100));
    } else {
        Sleep(std::min<DWORD>(remainingMs, 50));
    }
}

bool WriteCommand(HANDLE pipe, const std::wstring& command) {
    std::wstring payload = command + L"\n";
    DWORD bytesWritten = 0;
    DWORD bytesToWrite = static_cast<DWORD>(payload.size() * sizeof(wchar_t));
    BOOL ok = WriteFile(pipe, payload.data(), bytesToWrite, &bytesWritten, NULL);
    return ok && bytesWritten == bytesToWrite;
}

bool TryParseExitCodeResponse(const std::vector<char>& responseBytes, DWORD* exitCode, bool* hasCompleteLine) {
    *hasCompleteLine = false;
    size_t charCount = responseBytes.size() / sizeof(wchar_t);
    if (charCount == 0) return false;

    std::wstring response(reinterpret_cast<const wchar_t*>(responseBytes.data()), charCount);
    size_t newline = response.find(L'\n');
    if (newline == std::wstring::npos) return false;

    *hasCompleteLine = true;
    response.resize(newline);
    if (!response.empty() && response.back() == L'\r') response.pop_back();

    wchar_t* end = NULL;
    unsigned long value = wcstoul(response.c_str(), &end, 10);
    if (end == response.c_str()) return false;

    if (exitCode) *exitCode = static_cast<DWORD>(value);
    return true;
}

}

namespace capture_pipe {

bool SendCommand(const std::wstring& command, DWORD timeoutMs) {
    return SendCommandToPipe(kCapturePipeName, command, timeoutMs);
}

bool SendCommandToPipe(const std::wstring& pipeName, const std::wstring& command, DWORD timeoutMs) {
    std::wstring pipePath = PipePath(pipeName);
    DWORD startTick = GetTickCount();
    for (;;) {
        HANDLE pipe = CreateFileW(pipePath.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, NULL);
        if (pipe != INVALID_HANDLE_VALUE) {
            bool ok = WriteCommand(pipe, command);
            CloseHandle(pipe);
            return ok;
        }

        DWORD error = GetLastError();
        DWORD elapsed = ElapsedSince(startTick);
        if (elapsed >= timeoutMs) return false;

        WaitForNextPipeAttempt(error, timeoutMs - elapsed);
    }
}

bool SendCommandForExitCode(const std::wstring& command, DWORD timeoutMs, DWORD* exitCode) {
    return SendCommandForExitCodeToPipe(kCapturePipeName, command, timeoutMs, exitCode);
}

bool SendCommandForExitCodeToPipe(const std::wstring& pipeName,
                                  const std::wstring& command,
                                  DWORD timeoutMs,
                                  DWORD* exitCode) {
    if (exitCode) *exitCode = 1;

    std::wstring pipePath = PipePath(pipeName);
    DWORD startTick = GetTickCount();
    HANDLE pipe = INVALID_HANDLE_VALUE;
    for (;;) {
        pipe = CreateFileW(pipePath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
        if (pipe != INVALID_HANDLE_VALUE) break;

        DWORD elapsed = ElapsedSince(startTick);
        if (elapsed >= timeoutMs) return false;

        WaitForNextPipeAttempt(GetLastError(), timeoutMs - elapsed);
    }

    if (!WriteCommand(pipe, command)) {
        CloseHandle(pipe);
        return false;
    }

    std::vector<char> responseBytes;
    for (;;) {
        DWORD elapsed = ElapsedSince(startTick);
        if (elapsed >= timeoutMs) {
            CloseHandle(pipe);
            return false;
        }

        DWORD available = 0;
        if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL)) {
            CloseHandle(pipe);
            return false;
        }

        if (available == 0) {
            Sleep(5);
            continue;
        }

        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!ReadFile(pipe, buffer.data(), available, &read, NULL)) {
            CloseHandle(pipe);
            return false;
        }
        responseBytes.insert(responseBytes.end(), buffer.data(), buffer.data() + read);

        bool hasCompleteLine = false;
        bool parsed = TryParseExitCodeResponse(responseBytes, exitCode, &hasCompleteLine);
        if (!hasCompleteLine) continue;

        CloseHandle(pipe);
        return parsed;
    }
}

}
