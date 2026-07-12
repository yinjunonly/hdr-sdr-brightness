#include "native_pipe_server.h"

#include "capture_pipeline.h"
#include "native_capture_backend.h"
#include "native_common.h"

#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>

namespace native_capture {

namespace {

std::vector<std::wstring> SplitTabs(const std::wstring& text) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    for (;;) {
        size_t tab = text.find(L'\t', start);
        if (tab == std::wstring::npos) {
            parts.push_back(text.substr(start));
            return parts;
        }
        parts.push_back(text.substr(start, tab - start));
        start = tab + 1;
    }
}

int ParseInt(const std::vector<std::wstring>& parts, size_t index) {
    if (index >= parts.size()) return 0;
    wchar_t* end = nullptr;
    long value = std::wcstol(parts[index].c_str(), &end, 10);
    return end == parts[index].c_str() ? 0 : static_cast<int>(value);
}

float SdrWhiteFromLevel(int sdrWhiteLevel, float fallback) {
    if (sdrWhiteLevel <= 0) return fallback;
    return std::clamp(sdrWhiteLevel / 1000.0f, 0.1f, 10.0f);
}

bool ReadPipeLine(HANDLE pipe, std::wstring* line) {
    line->clear();
    wchar_t buffer[256] = {};
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(pipe, buffer, sizeof(buffer), &read, nullptr)) {
            return false;
        }
        if (read == 0) continue;

        size_t chars = read / sizeof(wchar_t);
        line->append(buffer, buffer + chars);
        size_t newline = line->find(L'\n');
        if (newline != std::wstring::npos) {
            line->resize(newline);
            if (!line->empty() && line->back() == L'\r') line->pop_back();
            if (!line->empty() && line->front() == 0xfeff) line->erase(line->begin());
            return true;
        }
    }
}

bool WriteExitCode(HANDLE pipe, int code) {
    std::wstring response = std::to_wstring(code) + L"\n";
    DWORD written = 0;
    DWORD bytes = static_cast<DWORD>(response.size() * sizeof(wchar_t));
    return WriteFile(pipe, response.data(), bytes, &written, nullptr) && written == bytes;
}

int HandlePipeCommand(const Options& serverOptions, NativeCaptureRuntime* runtime, const std::wstring& command) {
    std::vector<std::wstring> parts = SplitTabs(command);
    if (parts.empty()) return 2;

    if (_wcsicmp(parts[0].c_str(), L"select-region") == 0) {
        CaptureRequest request;
        request.sdrWhite = SdrWhiteFromLevel(ParseInt(parts, 2), serverOptions.sdrWhite);
        request.diagnostic = serverOptions.diagnostic;
        request.copyToClipboard = true;
        request.saveOutput = false;
        return RunSelectedRegionCapture(request, runtime);
    }

    if (_wcsicmp(parts[0].c_str(), L"capture-file") == 0) {
        CaptureRequest request;
        request.sdrWhite = SdrWhiteFromLevel(ParseInt(parts, 3), serverOptions.sdrWhite);
        request.diagnostic = serverOptions.diagnostic;
        request.copyToClipboard = false;
        request.saveOutput = parts.size() > 2 && !parts[2].empty();
        request.outputPath = request.saveOutput ? parts[2] : serverOptions.outputPath;
        return request.saveOutput ? RunCaptureRequest(request, runtime) : 2;
    }

    if (_wcsicmp(parts[0].c_str(), L"fullscreen-clip") != 0) {
        return 2;
    }

    CaptureRequest request;
    request.sdrWhite = SdrWhiteFromLevel(ParseInt(parts, 3), serverOptions.sdrWhite);
    request.diagnostic = serverOptions.diagnostic;
    request.copyToClipboard = true;
    request.saveOutput = parts.size() > 2 && !parts[2].empty();
    request.outputPath = request.saveOutput ? parts[2] : serverOptions.outputPath;

    return RunCaptureRequest(request, runtime);
}

std::wstring PipePath(const std::wstring& pipeName) {
    return L"\\\\.\\pipe\\" + pipeName;
}

DWORD WINAPI ParentMonitorThread(LPVOID param) {
    HANDLE parent = static_cast<HANDLE>(param);
    WaitForSingleObject(parent, INFINITE);
    CloseHandle(parent);
    ExitProcess(0);
    return 0;
}

void StartParentMonitor(DWORD parentPid) {
    if (parentPid == 0) return;

    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentPid);
    if (!parent) return;

    HANDLE thread = CreateThread(nullptr, 0, ParentMonitorThread, parent, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    } else {
        CloseHandle(parent);
    }
}

}  // namespace

int RunPipeServer(const Options& options) {
    std::wstring pipePath = PipePath(options.pipeName);
    StartParentMonitor(options.parentPid);
    NativeCaptureRuntime runtime;
    if (!InitializePrimaryMonitorRuntime(options.diagnostic, &runtime)) {
        return 8;
    }

    for (;;) {
        HANDLE pipe = CreateNamedPipeW(pipePath.c_str(),
                                       PIPE_ACCESS_DUPLEX,
                                       PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                       1,
                                       4096,
                                       4096,
                                       options.idleTimeoutMs > 0 ? options.idleTimeoutMs : 0,
                                       nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            std::fprintf(stderr, "CreateNamedPipe failed: %lu.\n", static_cast<unsigned long>(GetLastError()));
            return 8;
        }

        BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : GetLastError() == ERROR_PIPE_CONNECTED;
        if (!connected) {
            std::fprintf(stderr, "ConnectNamedPipe failed: %lu.\n", static_cast<unsigned long>(GetLastError()));
            CloseHandle(pipe);
            return 8;
        }

        std::wstring command;
        int code = ReadPipeLine(pipe, &command) ? HandlePipeCommand(options, &runtime, command) : 8;
        std::fflush(stdout);
        std::fflush(stderr);
        WriteExitCode(pipe, code);
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);

        if (options.serverOnce) return 0;
    }
}

}  // namespace native_capture
