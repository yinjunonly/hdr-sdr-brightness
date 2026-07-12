#include "capture_paths.h"

#include "process_util.h"

#include <windows.h>

#include <sstream>
#include <vector>

namespace capture_paths {

namespace {

std::wstring TempBaseDirectory() {
    std::vector<wchar_t> temp(MAX_PATH);
    DWORD length = GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
    return (length > 0 && length < temp.size()) ? std::wstring(temp.data(), length) : L".";
}

std::wstring TimestampName(const wchar_t* prefix, const SYSTEMTIME& time, DWORD suffix) {
    std::wstringstream name;
    name << prefix
         << time.wYear
         << (time.wMonth < 10 ? L"0" : L"") << time.wMonth
         << (time.wDay < 10 ? L"0" : L"") << time.wDay
         << L"-"
         << (time.wHour < 10 ? L"0" : L"") << time.wHour
         << (time.wMinute < 10 ? L"0" : L"") << time.wMinute
         << (time.wSecond < 10 ? L"0" : L"") << time.wSecond
         << L"-" << suffix
         << L".bmp";
    return name.str();
}

std::wstring CaptureTempPath(const wchar_t* directoryName, const std::wstring& filename) {
    std::wstring directory = JoinPath(TempBaseDirectory(), directoryName);
    CreateDirectoryW(directory.c_str(), NULL);
    return JoinPath(directory, filename);
}

}  // namespace

std::wstring FullscreenNotificationBmpPath() {
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    return CaptureTempPath(L"HdrSdrBrightness", TimestampName(L"fullscreen-", now, now.wMilliseconds));
}

std::wstring RegionEditBmpPath() {
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    return CaptureTempPath(L"HDR SDR Brightness", TimestampName(L"region-", now, GetTickCount()));
}

std::wstring ReplaceExtension(const std::wstring& path, const std::wstring& extension) {
    size_t slash = path.find_last_of(L"\\/");
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) {
        return path + extension;
    }
    return path.substr(0, dot) + extension;
}

}  // namespace capture_paths
