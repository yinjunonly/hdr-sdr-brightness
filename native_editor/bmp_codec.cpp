#include "bmp_codec.h"

#include <windows.h>

#include <cstdint>
#include <limits>
#include <utility>

namespace editor {

namespace {

bool Fail(const wchar_t* message, std::wstring* error) {
    if (error) *error = message;
    return false;
}

class FileHandle {
public:
    explicit FileHandle(HANDLE value) : value_(value) {}
    ~FileHandle() { if (value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
    HANDLE Get() const { return value_; }
private:
    HANDLE value_;
};

bool ReadExact(HANDLE file, void* destination, DWORD size) {
    DWORD read = 0;
    return size == 0 || (ReadFile(file, destination, size, &read, nullptr) && read == size);
}

bool Seek(HANDLE file, uint64_t offset) {
    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    return SetFilePointerEx(file, position, nullptr, FILE_BEGIN) != FALSE;
}

void ForceOpaqueAlpha(BgraImage* image) {
    for (size_t index = 3; index < image->pixels.size(); index += 4) {
        image->pixels[index] = 255;
    }
}

}  // namespace

bool LoadBmp(const std::wstring& path, BgraImage* image, std::wstring* error) {
    if (!image) return Fail(L"No image destination was provided.", error);
    *image = BgraImage{};
    if (error) error->clear();

    FileHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                nullptr));
    if (file.Get() == INVALID_HANDLE_VALUE) return Fail(L"Could not open the image file.", error);

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(file.Get(), &fileSize) || fileSize.QuadPart < 0) {
        return Fail(L"Could not inspect the image file.", error);
    }
    if (fileSize.QuadPart < static_cast<LONGLONG>(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))) {
        return Fail(L"The BMP header is truncated.", error);
    }

    BITMAPFILEHEADER fileHeader{};
    BITMAPINFOHEADER info{};
    if (!ReadExact(file.Get(), &fileHeader, sizeof(fileHeader)) ||
        !ReadExact(file.Get(), &info, sizeof(info))) {
        return Fail(L"Could not read the BMP header.", error);
    }
    if (fileHeader.bfType != 0x4d42 || info.biSize < sizeof(BITMAPINFOHEADER)) {
        return Fail(L"The image is not a supported BMP.", error);
    }
    if (info.biWidth <= 0 || info.biHeight == 0 || info.biHeight == LONG_MIN ||
        info.biPlanes != 1 || info.biBitCount != 32 || info.biCompression != BI_RGB) {
        return Fail(L"Only uncompressed 32-bit BMP images are supported.", error);
    }

    uint64_t width = static_cast<uint64_t>(info.biWidth);
    uint64_t height = static_cast<uint64_t>(info.biHeight < 0 ? -info.biHeight : info.biHeight);
    uint64_t rowBytes = width * 4;
    uint64_t pixelBytes = rowBytes * height;
    uint64_t pixelOffset = fileHeader.bfOffBits;
    uint64_t totalSize = static_cast<uint64_t>(fileSize.QuadPart);
    if (width > std::numeric_limits<UINT>::max() ||
        height > std::numeric_limits<UINT>::max() ||
        rowBytes > std::numeric_limits<DWORD>::max() ||
        pixelBytes > std::numeric_limits<DWORD>::max() ||
        pixelBytes > SIZE_MAX || pixelOffset > totalSize || pixelBytes > totalSize - pixelOffset) {
        return Fail(L"The BMP pixel data is invalid or truncated.", error);
    }

    BgraImage loaded;
    loaded.width = static_cast<UINT>(width);
    loaded.height = static_cast<UINT>(height);
    loaded.pixels.resize(static_cast<size_t>(pixelBytes));
    if (!Seek(file.Get(), pixelOffset)) return Fail(L"Could not seek to the BMP pixels.", error);

    if (info.biHeight < 0) {
        if (!ReadExact(file.Get(), loaded.pixels.data(), static_cast<DWORD>(pixelBytes))) {
            return Fail(L"Could not read the BMP pixels.", error);
        }
    } else {
        for (UINT fileRow = 0; fileRow < loaded.height; ++fileRow) {
            UINT destinationRow = loaded.height - 1 - fileRow;
            BYTE* destination = loaded.pixels.data() + static_cast<size_t>(destinationRow) * rowBytes;
            if (!ReadExact(file.Get(), destination, static_cast<DWORD>(rowBytes))) {
                return Fail(L"Could not read the BMP pixels.", error);
            }
        }
    }
    ForceOpaqueAlpha(&loaded);
    if (!loaded.IsValid()) return Fail(L"The decoded BMP dimensions are invalid.", error);
    *image = std::move(loaded);
    return true;
}

}  // namespace editor
