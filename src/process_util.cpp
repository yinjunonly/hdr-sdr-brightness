#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>

#include "process_util.h"

std::wstring QuotePath(const std::wstring& path) {
    return L"\"" + path + L"\"";
}

std::wstring QuoteCommandLineArgument(const std::wstring& value) {
    std::wstring quoted;
    quoted.reserve(value.size() + 2);
    quoted.push_back(L'"');

    size_t backslashes = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        wchar_t ch = value[i];
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }

        if (ch == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(ch);
            backslashes = 0;
            continue;
        }

        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(ch);
    }

    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::wstring QuotePowerShellString(const std::wstring& value) {
    std::wstring quoted;
    quoted.reserve(value.size() + 2);
    quoted.push_back(L'\'');
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == L'\'') quoted.push_back(L'\'');
        quoted.push_back(value[i]);
    }
    quoted.push_back(L'\'');
    return quoted;
}

std::wstring GetExePath() {
    std::vector<wchar_t> path(MAX_PATH);
    DWORD length = 0;
    for (;;) {
        length = GetModuleFileNameW(NULL, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) return L"";
        if (length < path.size() - 1) break;
        path.resize(path.size() * 2);
    }
    return std::wstring(path.data(), length);
}

std::wstring DirectoryFromPath(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L"";
    return path.substr(0, pos);
}

std::wstring JoinPath(const std::wstring& directory, const std::wstring& relative) {
    if (directory.empty()) return relative;
    wchar_t last = directory[directory.size() - 1];
    if (last == L'\\' || last == L'/') return directory + relative;
    return directory + L"\\" + relative;
}

bool FileExists(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

namespace {

bool IsWow64ProcessCurrent() {
    typedef BOOL(WINAPI* IsWow64ProcessFn)(HANDLE, PBOOL);
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) return false;

    IsWow64ProcessFn fn = reinterpret_cast<IsWow64ProcessFn>(GetProcAddress(kernel32, "IsWow64Process"));
    if (!fn) return false;

    BOOL isWow64 = FALSE;
    if (!fn(GetCurrentProcess(), &isWow64)) return false;
    return isWow64 != FALSE;
}

std::wstring GetWindowsDirectoryPath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    UINT length = GetWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    if (length == 0) return L"C:\\Windows";
    if (length >= buffer.size()) {
        buffer.resize(length + 1);
        length = GetWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    }
    return std::wstring(buffer.data(), length);
}

bool ReadPipeAvailable(HANDLE pipe, std::string* output) {
    DWORD available = 0;
    if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL)) return false;
    if (available == 0) return true;

    std::vector<char> buffer(available);
    DWORD read = 0;
    if (!ReadFile(pipe, buffer.data(), available, &read, NULL)) return false;
    output->append(buffer.data(), buffer.data() + read);
    return true;
}

}

std::wstring GetCloudSettingsReaderPath() {
    std::wstring windows = GetWindowsDirectoryPath();
    std::wstring sysnative = windows + L"\\Sysnative\\readCloudDataSettings.exe";
    if (IsWow64ProcessCurrent() && FileExists(sysnative)) return sysnative;

    std::wstring system32 = windows + L"\\System32\\readCloudDataSettings.exe";
    if (FileExists(system32)) return system32;

    return L"";
}

bool LaunchDetached(const std::wstring& commandLine, const std::wstring& workingDirectory) {
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    BOOL created = CreateProcessW(NULL, mutableCommand.data(), NULL, NULL, FALSE, 0,
                                  NULL, workingDirectory.empty() ? NULL : workingDirectory.c_str(), &si, &pi);
    if (!created) return false;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool LaunchDetachedHidden(const std::wstring& commandLine, const std::wstring& workingDirectory) {
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    BOOL created = CreateProcessW(NULL, mutableCommand.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW,
                                  NULL, workingDirectory.empty() ? NULL : workingDirectory.c_str(), &si, &pi);
    if (!created) return false;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool RunProcessCapture(const std::wstring& commandLine, DWORD timeoutMs, std::string* output) {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = NULL;
    HANDLE writePipe = NULL;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return false;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    BOOL created = CreateProcessW(NULL, mutableCommand.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW,
                                  NULL, NULL, &si, &pi);
    CloseHandle(writePipe);
    if (!created) {
        CloseHandle(readPipe);
        return false;
    }

    DWORD startTick = GetTickCount();
    bool timedOut = false;
    for (;;) {
        ReadPipeAvailable(readPipe, output);
        DWORD wait = WaitForSingleObject(pi.hProcess, 25);
        if (wait == WAIT_OBJECT_0) break;
        if (GetTickCount() - startTick > timeoutMs) {
            timedOut = true;
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 500);
            break;
        }
    }
    ReadPipeAvailable(readPipe, output);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(readPipe);

    return !timedOut && exitCode == 0;
}

bool RunHiddenCommand(const std::wstring& commandLine, DWORD timeoutMs) {
    DWORD exitCode = 1;
    return RunHiddenCommandExitCode(commandLine, timeoutMs, &exitCode) && exitCode == 0;
}

bool RunHiddenCommandExitCode(const std::wstring& commandLine, DWORD timeoutMs, DWORD* exitCode) {
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    BOOL created = CreateProcessW(NULL, mutableCommand.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW,
                                  NULL, NULL, &si, &pi);
    if (!created) return false;

    DWORD wait = WaitForSingleObject(pi.hProcess, timeoutMs);
    DWORD processExitCode = 1;
    if (wait == WAIT_OBJECT_0) {
        GetExitCodeProcess(pi.hProcess, &processExitCode);
    } else {
        TerminateProcess(pi.hProcess, 1);
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (exitCode) *exitCode = processExitCode;
    return wait == WAIT_OBJECT_0;
}
