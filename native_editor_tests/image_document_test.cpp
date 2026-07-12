#include "../native_editor/bmp_codec.h"
#include "../native_editor/image_document.h"

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

namespace {

std::wstring FixturePath(const wchar_t* suffix) {
    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    return std::wstring(temp) + L"hdr-sdr-native-editor-" +
        std::to_wstring(GetCurrentProcessId()) + suffix;
}

bool WriteBytes(const std::wstring& path, const std::vector<BYTE>& bytes) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
        written == bytes.size();
    CloseHandle(file);
    return ok;
}

bool WriteBmpFixture(const std::wstring& path, bool topDown) {
    const LONG width = 4;
    const LONG height = 3;
    const DWORD pixelBytes = width * height * 4;

    BITMAPFILEHEADER fileHeader{};
    fileHeader.bfType = 0x4d42;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + pixelBytes;

    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = width;
    info.biHeight = topDown ? -height : height;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    info.biSizeImage = pixelBytes;

    std::vector<BYTE> bytes(fileHeader.bfSize);
    std::memcpy(bytes.data(), &fileHeader, sizeof(fileHeader));
    std::memcpy(bytes.data() + sizeof(fileHeader), &info, sizeof(info));
    BYTE* pixels = bytes.data() + fileHeader.bfOffBits;
    for (LONG fileY = 0; fileY < height; ++fileY) {
        LONG physicalY = topDown ? fileY : height - 1 - fileY;
        for (LONG x = 0; x < width; ++x) {
            size_t index = (static_cast<size_t>(fileY) * width + x) * 4;
            pixels[index] = static_cast<BYTE>(30 + physicalY);
            pixels[index + 1] = static_cast<BYTE>(20 + x);
            pixels[index + 2] = static_cast<BYTE>(10 + physicalY);
            pixels[index + 3] = 0;
        }
    }
    return WriteBytes(path, bytes);
}

bool TestBmpOrientation(bool topDown) {
    std::wstring path = FixturePath(topDown ? L"-top-down.bmp" : L"-bottom-up.bmp");
    if (!WriteBmpFixture(path, topDown)) return false;

    editor::BgraImage image;
    std::wstring error;
    bool loaded = editor::LoadBmp(path, &image, &error);
    DeleteFileW(path.c_str());
    if (!loaded) {
        std::fwprintf(stderr, L"FAIL: BMP load failed: %ls\n", error.c_str());
        return false;
    }

    size_t bottomCenter = (static_cast<size_t>(2) * 4 + 2) * 4;
    bool passed = image.width == 4 && image.height == 3 &&
        image.pixels[bottomCenter] == 32 &&
        image.pixels[bottomCenter + 1] == 22 &&
        image.pixels[bottomCenter + 2] == 12 &&
        image.pixels[bottomCenter + 3] == 255;
    if (!passed) {
        std::fprintf(stderr, "FAIL: BMP physical row order or alpha was changed.\n");
    }
    return passed;
}

bool TestTruncatedBmpRejected() {
    std::wstring path = FixturePath(L"-truncated.bmp");
    std::vector<BYTE> bytes(20, 0);
    if (!WriteBytes(path, bytes)) return false;
    editor::BgraImage image;
    std::wstring error;
    bool loaded = editor::LoadBmp(path, &image, &error);
    DeleteFileW(path.c_str());
    if (loaded || error.empty()) {
        std::fprintf(stderr, "FAIL: truncated BMP was accepted without an error.\n");
        return false;
    }
    return true;
}

bool TestAdjustmentParity() {
    editor::BgraImage source;
    source.width = 1;
    source.height = 1;
    source.pixels = {200, 200, 200, 255};
    editor::ImageDocument document(source);

    editor::BgraImage balanced = document.Render();
    document.SetAdjustmentPreset(editor::AdjustmentPreset::Low);
    editor::BgraImage low = document.Render();
    document.SetAdjustmentPreset(editor::AdjustmentPreset::High);
    editor::BgraImage high = document.Render();

    if (balanced.pixels[0] != 200 || low.pixels[0] != 176 || high.pixels[0] != 210 ||
        low.pixels[3] != 255 || high.pixels[3] != 255) {
        std::fprintf(stderr,
                     "FAIL: adjustment parity mismatch: balanced=%u low=%u high=%u.\n",
                     balanced.pixels[0], low.pixels[0], high.pixels[0]);
        return false;
    }
    return true;
}

bool TestEditHistory() {
    editor::BgraImage source;
    source.width = 2;
    source.height = 2;
    source.pixels.assign(16, 255);
    editor::ImageDocument document(source);

    editor::EditOperation operation;
    operation.type = editor::EditOperationType::Marker;
    operation.rect = RECT{0, 0, 2, 2};
    operation.color = RGB(255, 59, 48);
    operation.strokeWidth = 4;
    document.AddOperation(operation);
    if (document.OperationCount() != 1 || !document.CanUndo() || document.CanRedo()) return false;
    if (!document.Undo() || document.OperationCount() != 0 || !document.CanRedo()) return false;
    if (!document.Redo() || document.OperationCount() != 1 || !document.CanUndo()) return false;
    document.Reset();
    if (document.OperationCount() != 0 || document.CanUndo() || document.CanRedo() ||
        document.GetAdjustmentPreset() != editor::AdjustmentPreset::Balanced) {
        std::fprintf(stderr, "FAIL: document reset did not restore the initial edit state.\n");
        return false;
    }
    return true;
}

editor::BgraImage SolidImage(UINT width, UINT height, BYTE b, BYTE g, BYTE r) {
    editor::BgraImage image;
    image.width = width;
    image.height = height;
    image.pixels.resize(static_cast<size_t>(width) * height * 4);
    for (size_t index = 0; index < image.pixels.size(); index += 4) {
        image.pixels[index] = b;
        image.pixels[index + 1] = g;
        image.pixels[index + 2] = r;
        image.pixels[index + 3] = 255;
    }
    return image;
}

const BYTE* Pixel(const editor::BgraImage& image, UINT x, UINT y) {
    return image.pixels.data() + (static_cast<size_t>(y) * image.width + x) * 4;
}

bool IsMostlyRed(const BYTE* pixel) {
    return pixel[2] > 200 && pixel[1] < 100 && pixel[0] < 100 && pixel[3] == 255;
}

bool TestCropUsesPhysicalPixels() {
    editor::BgraImage source = SolidImage(4, 3, 0, 0, 0);
    for (UINT y = 0; y < source.height; ++y) {
        for (UINT x = 0; x < source.width; ++x) {
            BYTE* pixel = source.pixels.data() + (static_cast<size_t>(y) * source.width + x) * 4;
            pixel[0] = static_cast<BYTE>(x + y * 10);
        }
    }
    editor::BgraImage crop;
    if (!editor::CropImage(source, RECT{1, 1, 4, 3}, &crop) ||
        crop.width != 3 || crop.height != 2 ||
        Pixel(crop, 0, 0)[0] != 11 || Pixel(crop, 2, 1)[0] != 23) {
        std::fprintf(stderr, "FAIL: physical-pixel crop mapping changed.\n");
        return false;
    }
    return true;
}

bool TestVectorAnnotationsRender() {
    editor::ImageDocument document(SolidImage(64, 64, 20, 30, 40));

    editor::EditOperation marker;
    marker.type = editor::EditOperationType::Marker;
    marker.rect = RECT{6, 6, 30, 30};
    marker.color = RGB(255, 59, 48);
    marker.strokeWidth = 4;
    document.AddOperation(marker);

    editor::EditOperation ellipse = marker;
    ellipse.type = editor::EditOperationType::Ellipse;
    ellipse.rect = RECT{34, 6, 58, 30};
    document.AddOperation(ellipse);

    editor::EditOperation pen = marker;
    pen.type = editor::EditOperationType::Pen;
    pen.points = {POINT{8, 48}, POINT{28, 48}, editor::PathBreakPoint(), POINT{36, 48}, POINT{56, 48}};
    pen.strokeWidth = 6;
    document.AddOperation(pen);

    editor::BgraImage output = document.Render();
    if (!IsMostlyRed(Pixel(output, 6, 18)) ||
        !IsMostlyRed(Pixel(output, 46, 6)) ||
        !IsMostlyRed(Pixel(output, 18, 48)) ||
        !IsMostlyRed(Pixel(output, 46, 48)) ||
        IsMostlyRed(Pixel(output, 32, 48))) {
        std::fprintf(stderr, "FAIL: native vector annotation pixels are incorrect.\n");
        return false;
    }
    return true;
}

bool TestMosaicRendersAndClips() {
    editor::BgraImage source = SolidImage(32, 24, 0, 0, 0);
    for (UINT y = 0; y < source.height; ++y) {
        for (UINT x = 0; x < source.width; ++x) {
            BYTE* pixel = source.pixels.data() + (static_cast<size_t>(y) * source.width + x) * 4;
            pixel[0] = static_cast<BYTE>((x * 17 + y * 3) & 255);
            pixel[1] = static_cast<BYTE>((x * 5 + y * 19) & 255);
            pixel[2] = static_cast<BYTE>((x * 11 + y * 7) & 255);
        }
    }

    editor::ImageDocument document(source);
    editor::EditOperation mosaic;
    mosaic.type = editor::EditOperationType::Mosaic;
    mosaic.rect = RECT{-4, 4, 20, 20};
    mosaic.strokeWidth = 8;
    document.AddOperation(mosaic);
    editor::BgraImage output = document.Render();

    const BYTE* unchanged = Pixel(output, 28, 8);
    const BYTE* original = Pixel(source, 28, 8);
    const BYTE* blockA = Pixel(output, 2, 6);
    const BYTE* blockB = Pixel(output, 6, 10);
    if (std::memcmp(unchanged, original, 4) != 0 ||
        std::memcmp(blockA, blockB, 3) != 0 ||
        std::memcmp(Pixel(output, 2, 6), Pixel(source, 2, 6), 3) == 0) {
        std::fprintf(stderr, "FAIL: mosaic output or clipping is incorrect.\n");
        return false;
    }
    return true;
}

bool TestMosaicBrushUsesStableSourceGrid() {
    editor::BgraImage source = SolidImage(128, 80, 0, 0, 0);
    for (UINT y = 0; y < source.height; ++y) {
        for (UINT x = 0; x < source.width; ++x) {
            BYTE* pixel = source.pixels.data() +
                (static_cast<size_t>(y) * source.width + x) * 4;
            pixel[0] = static_cast<BYTE>(x * 2);
            pixel[1] = static_cast<BYTE>(y * 3);
            pixel[2] = static_cast<BYTE>(x + y);
            pixel[3] = 255;
        }
    }

    editor::ImageDocument document(source);
    editor::EditOperation mosaic;
    mosaic.type = editor::EditOperationType::Mosaic;
    mosaic.points = {POINT{20, 40}, POINT{108, 40}};
    mosaic.strokeWidth = 28;
    document.AddOperation(mosaic);
    editor::BgraImage output = document.Render();

    const BYTE expected[] = {98, 120, 89, 255};
    for (UINT y = 36; y <= 44; ++y) {
        for (UINT x = 45; x <= 53; ++x) {
            if (std::memcmp(Pixel(output, x, y), expected, 4) != 0) {
                std::fprintf(stderr,
                    "FAIL: mosaic brush cell is smeared instead of using its stable source-grid average.\n");
                return false;
            }
        }
    }
    if (std::memcmp(Pixel(output, 49, 10), Pixel(source, 49, 10), 4) != 0) {
        std::fprintf(stderr, "FAIL: mosaic brush changed pixels outside the stroke.\n");
        return false;
    }
    return true;
}

}  // namespace

int main() {
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) return 1;
    bool passed = TestBmpOrientation(true) &&
        TestBmpOrientation(false) &&
        TestTruncatedBmpRejected() &&
        TestAdjustmentParity() &&
        TestEditHistory() &&
        TestCropUsesPhysicalPixels() &&
        TestVectorAnnotationsRender() &&
        TestMosaicRendersAndClips() &&
        TestMosaicBrushUsesStableSourceGrid();
    Gdiplus::GdiplusShutdown(token);
    if (!passed) return 1;
    std::printf("PASS: native editor image loading, adjustment, and edit history are stable.\n");
    return 0;
}
