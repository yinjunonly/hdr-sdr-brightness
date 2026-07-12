#pragma once

#include <windows.h>
#include <windows.foundation.h>
#include <winstring.h>

#include <cstdio>

namespace native_capture {

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { Reset(); }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    T* Get() const { return value_; }
    T** Put() {
        Reset();
        return &value_;
    }
    T* operator->() const { return value_; }
    explicit operator bool() const { return value_ != nullptr; }

    void CopyFrom(T* value) {
        Reset();
        value_ = value;
        if (value_) {
            value_->AddRef();
        }
    }

    void Reset() {
        if (value_) {
            value_->Release();
            value_ = nullptr;
        }
    }

private:
    T* value_ = nullptr;
};

inline HRESULT CloseWinRtObject(IInspectable* value) {
    if (!value) return S_OK;

    ComPtr<ABI::Windows::Foundation::IClosable> closable;
    HRESULT hr = value->QueryInterface(
        IID___x_ABI_CWindows_CFoundation_CIClosable,
        reinterpret_cast<void**>(closable.Put()));
    if (hr == E_NOINTERFACE) return S_OK;
    if (FAILED(hr)) return hr;
    return closable->Close();
}

class WinRtCloseGuard {
public:
    explicit WinRtCloseGuard(IInspectable* value) : value_(value) {}
    ~WinRtCloseGuard() { CloseWinRtObject(value_); }

    WinRtCloseGuard(const WinRtCloseGuard&) = delete;
    WinRtCloseGuard& operator=(const WinRtCloseGuard&) = delete;

private:
    IInspectable* value_;
};

class HString {
public:
    explicit HString(const wchar_t* value) {
        if (value) {
            WindowsCreateString(value, static_cast<UINT32>(lstrlenW(value)), &value_);
        }
    }

    ~HString() {
        if (value_) WindowsDeleteString(value_);
    }

    HString(const HString&) = delete;
    HString& operator=(const HString&) = delete;

    HSTRING Get() const { return value_; }

private:
    HSTRING value_ = nullptr;
};

class Stopwatch {
public:
    Stopwatch() {
        QueryPerformanceFrequency(&frequency_);
        Reset();
    }

    void Reset() { QueryPerformanceCounter(&start_); }

    double ElapsedMs() const {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (now.QuadPart - start_.QuadPart) * 1000.0 / frequency_.QuadPart;
    }

private:
    LARGE_INTEGER frequency_{};
    LARGE_INTEGER start_{};
};

inline bool Check(HRESULT hr, const char* operation) {
    if (SUCCEEDED(hr)) return true;
    std::fprintf(stderr, "%s failed: 0x%08lx\n", operation, static_cast<unsigned long>(hr));
    return false;
}

}  // namespace native_capture
