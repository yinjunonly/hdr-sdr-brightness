#include "wic_png.h"

#include "com_ptr.h"

#include <windows.h>
#include <objidl.h>
#include <wincodec.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace editor {

namespace {

bool Fail(const wchar_t* operation, HRESULT hr, std::wstring* error) {
    if (error) {
        wchar_t message[160] = {};
        swprintf(message, 160, L"%ls failed (0x%08lx).", operation,
                 static_cast<unsigned long>(hr));
        *error = message;
    }
    return false;
}

bool CopyStreamBytes(IStream* stream, std::vector<BYTE>* bytes, std::wstring* error) {
    STATSTG stats{};
    HRESULT hr = stream->Stat(&stats, STATFLAG_NONAME);
    if (FAILED(hr) || stats.cbSize.QuadPart <= 0 ||
        static_cast<ULONGLONG>(stats.cbSize.QuadPart) > SIZE_MAX) {
        return Fail(L"PNG stream size", FAILED(hr) ? hr : E_FAIL, error);
    }
    HGLOBAL memory = nullptr;
    hr = GetHGlobalFromStream(stream, &memory);
    if (FAILED(hr) || !memory) return Fail(L"GetHGlobalFromStream", hr, error);

    SIZE_T allocationSize = GlobalSize(memory);
    SIZE_T size = static_cast<SIZE_T>(stats.cbSize.QuadPart);
    if (allocationSize < size) return Fail(L"GlobalSize", E_FAIL, error);
    void* raw = GlobalLock(memory);
    if (!raw) return Fail(L"GlobalLock", HRESULT_FROM_WIN32(GetLastError()), error);
    bytes->resize(size);
    std::memcpy(bytes->data(), raw, size);
    GlobalUnlock(memory);
    return true;
}

}  // namespace

bool EncodePng(const BgraImage& image, std::vector<BYTE>* png, std::wstring* error) {
    if (error) error->clear();
    if (!png) return Fail(L"PNG destination", E_POINTER, error);
    png->clear();
    if (!image.IsValid()) return Fail(L"Image validation", E_INVALIDARG, error);

    uint64_t stride64 = static_cast<uint64_t>(image.width) * 4;
    uint64_t bytes64 = stride64 * image.height;
    if (stride64 > std::numeric_limits<UINT>::max() ||
        bytes64 > std::numeric_limits<UINT>::max()) {
        return Fail(L"Image size", E_INVALIDARG, error);
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IWICImagingFactory,
                                  reinterpret_cast<void**>(factory.Put()));
    if (FAILED(hr)) return Fail(L"WIC factory creation", hr, error);

    ComPtr<IStream> stream;
    hr = CreateStreamOnHGlobal(nullptr, TRUE, stream.Put());
    if (FAILED(hr)) return Fail(L"PNG memory stream creation", hr, error);

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.Put());
    if (FAILED(hr)) return Fail(L"PNG encoder creation", hr, error);
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) return Fail(L"PNG encoder initialization", hr, error);

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    hr = encoder->CreateNewFrame(frame.Put(), properties.Put());
    if (FAILED(hr)) return Fail(L"PNG frame creation", hr, error);
    hr = frame->Initialize(properties.Get());
    if (FAILED(hr)) return Fail(L"PNG frame initialization", hr, error);
    hr = frame->SetSize(image.width, image.height);
    if (FAILED(hr)) return Fail(L"PNG frame sizing", hr, error);

    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr) || !IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA)) {
        return Fail(L"PNG BGRA pixel format", FAILED(hr) ? hr : E_FAIL, error);
    }

    hr = frame->WritePixels(image.height,
                            static_cast<UINT>(stride64),
                            static_cast<UINT>(bytes64),
                            const_cast<BYTE*>(image.pixels.data()));
    if (FAILED(hr)) return Fail(L"PNG pixel write", hr, error);
    hr = frame->Commit();
    if (FAILED(hr)) return Fail(L"PNG frame commit", hr, error);
    hr = encoder->Commit();
    if (FAILED(hr)) return Fail(L"PNG encoder commit", hr, error);
    return CopyStreamBytes(stream.Get(), png, error);
}

bool SavePng(const std::wstring& path, const BgraImage& image, std::wstring* error) {
    std::vector<BYTE> png;
    if (!EncodePng(image, &png, error)) return false;
    if (png.size() > std::numeric_limits<DWORD>::max()) {
        return Fail(L"PNG file size", E_INVALIDARG, error);
    }

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return Fail(L"PNG file creation", HRESULT_FROM_WIN32(GetLastError()), error);
    }
    DWORD written = 0;
    bool ok = WriteFile(file, png.data(), static_cast<DWORD>(png.size()), &written, nullptr) &&
        written == png.size();
    DWORD writeError = ok ? ERROR_SUCCESS : GetLastError();
    CloseHandle(file);
    if (!ok) return Fail(L"PNG file write", HRESULT_FROM_WIN32(writeError), error);
    return true;
}

}  // namespace editor
