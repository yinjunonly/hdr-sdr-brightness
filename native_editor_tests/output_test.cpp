#include "../native_editor/image_document.h"
#include "../native_editor/wic_png.h"

#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

namespace {

UINT ReadBigEndian32(const BYTE* value) {
    return (static_cast<UINT>(value[0]) << 24) |
        (static_cast<UINT>(value[1]) << 16) |
        (static_cast<UINT>(value[2]) << 8) |
        static_cast<UINT>(value[3]);
}

std::wstring TempPngPath() {
    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    return std::wstring(temp) + L"hdr-sdr-native-editor-" +
        std::to_wstring(GetCurrentProcessId()) + L".png";
}

}  // namespace

int main() {
    HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) {
        std::fprintf(stderr, "FAIL: COM initialization failed.\n");
        return 1;
    }

    editor::BgraImage image;
    image.width = 2;
    image.height = 3;
    image.pixels = {
        10, 20, 30, 255,  40, 50, 60, 255,
        70, 80, 90, 255,  100, 110, 120, 255,
        130, 140, 150, 255,  160, 170, 180, 255
    };

    std::vector<BYTE> png;
    std::wstring error;
    bool encoded = editor::EncodePng(image, &png, &error);
    const BYTE signature[] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    bool headerOk = encoded && png.size() >= 24 &&
        std::equal(std::begin(signature), std::end(signature), png.begin()) &&
        ReadBigEndian32(png.data() + 16) == image.width &&
        ReadBigEndian32(png.data() + 20) == image.height;
    if (!headerOk) {
        std::fwprintf(stderr, L"FAIL: WIC PNG output is invalid: %ls\n", error.c_str());
        if (SUCCEEDED(init)) CoUninitialize();
        return 1;
    }

    std::wstring path = TempPngPath();
    if (!editor::SavePng(path, image, &error)) {
        std::fwprintf(stderr, L"FAIL: PNG save failed: %ls\n", error.c_str());
        if (SUCCEEDED(init)) CoUninitialize();
        return 1;
    }
    LARGE_INTEGER size{};
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    bool saved = file != INVALID_HANDLE_VALUE && GetFileSizeEx(file, &size) &&
        size.QuadPart == static_cast<LONGLONG>(png.size());
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    DeleteFileW(path.c_str());
    if (SUCCEEDED(init)) CoUninitialize();
    if (!saved) {
        std::fprintf(stderr, "FAIL: saved PNG bytes differ from encoded output.\n");
        return 1;
    }

    std::printf("PASS: native WIC PNG encoding preserves image dimensions and file bytes.\n");
    return 0;
}
