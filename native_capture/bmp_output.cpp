#include "bmp_output.h"

#include "native_common.h"

namespace native_capture {

namespace {

bool EnsureParentDirectory(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos || slash == 0) return true;
    std::wstring directory = path.substr(0, slash);
    if (CreateDirectoryW(directory.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) return true;

    size_t parentSlash = directory.find_last_of(L"\\/");
    if (parentSlash != std::wstring::npos && parentSlash > 0) {
        if (!EnsureParentDirectory(directory)) return false;
        if (CreateDirectoryW(directory.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) return true;
    }
    return false;
}

}  // namespace

bool SaveTopDownBmp(const std::wstring& path, UINT width, UINT height, const std::vector<BYTE>& bgra) {
    if (!EnsureParentDirectory(path)) {
        std::fwprintf(stderr, L"Could not create output directory for %ls\n", path.c_str());
        return false;
    }

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        std::fwprintf(stderr, L"Could not open output: %ls\n", path.c_str());
        return false;
    }

    if (bgra.size() > 0xffffffffu) {
        std::fprintf(stderr, "BMP output is too large.\n");
        CloseHandle(file);
        return false;
    }

    BITMAPFILEHEADER fileHeader = {};
    BITMAPINFOHEADER infoHeader = {};
    DWORD pixelBytes = static_cast<DWORD>(bgra.size());
    fileHeader.bfType = 0x4d42;
    fileHeader.bfOffBits = sizeof(fileHeader) + sizeof(infoHeader);
    fileHeader.bfSize = fileHeader.bfOffBits + pixelBytes;
    infoHeader.biSize = sizeof(infoHeader);
    infoHeader.biWidth = static_cast<LONG>(width);
    infoHeader.biHeight = -static_cast<LONG>(height);
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 32;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = pixelBytes;

    DWORD written = 0;
    bool ok = WriteFile(file, &fileHeader, sizeof(fileHeader), &written, nullptr) &&
              WriteFile(file, &infoHeader, sizeof(infoHeader), &written, nullptr) &&
              WriteFile(file, bgra.data(), pixelBytes, &written, nullptr);
    CloseHandle(file);
    return ok;
}

}  // namespace native_capture
