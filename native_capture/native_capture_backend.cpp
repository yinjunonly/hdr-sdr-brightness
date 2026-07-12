#define INITGUID

#include "native_capture_backend.h"

#include <initguid.h>
#include <inspectable.h>
#include <roapi.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.h>

extern "C" HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(IDXGIDevice* dxgiDevice,
                                                                   IInspectable** graphicsDevice);

DEFINE_GUID(IID_IDirect3DDxgiInterfaceAccess,
            0xa9b3d012, 0x3df2, 0x4ee3, 0xb8, 0xd1, 0x86, 0x95, 0xf4, 0x57, 0xd3, 0xc1);

struct IDirect3DDxgiInterfaceAccess : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetInterface(REFIID iid, void** object) = 0;
};

namespace native_capture {

namespace {

void DisableCaptureChrome(ABI::Windows::Graphics::Capture::IGraphicsCaptureSession* session) {
    ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession2> session2;
    if (SUCCEEDED(session->QueryInterface(IID___x_ABI_CWindows_CGraphics_CCapture_CIGraphicsCaptureSession2,
                                          reinterpret_cast<void**>(session2.Put())))) {
        session2->put_IsCursorCaptureEnabled(false);
    }

    ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession3> session3;
    if (SUCCEEDED(session->QueryInterface(IID___x_ABI_CWindows_CGraphics_CCapture_CIGraphicsCaptureSession3,
                                          reinterpret_cast<void**>(session3.Put())))) {
        session3->put_IsBorderRequired(false);
    }
}

bool CreatePrimaryMonitorItem(ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>* item) {
    HString itemClass(L"Windows.Graphics.Capture.GraphicsCaptureItem");
    ComPtr<IGraphicsCaptureItemInterop> interop;
    HRESULT hr = RoGetActivationFactory(itemClass.Get(), IID_IGraphicsCaptureItemInterop,
                                        reinterpret_cast<void**>(interop.Put()));
    if (!Check(hr, "RoGetActivationFactory(GraphicsCaptureItem)")) return false;

    HMONITOR monitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    hr = interop->CreateForMonitor(monitor,
                                   IID___x_ABI_CWindows_CGraphics_CCapture_CIGraphicsCaptureItem,
                                   reinterpret_cast<void**>(item->Put()));
    return Check(hr, "IGraphicsCaptureItemInterop::CreateForMonitor") && *item;
}

bool CreateD3D(ComPtr<ID3D11Device>* device,
               ComPtr<ID3D11DeviceContext>* context,
               ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>* winrtDevice) {
    D3D_FEATURE_LEVEL requestedLevel = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL actualLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   &requestedLevel, 1, D3D11_SDK_VERSION, device->Put(), &actualLevel, context->Put());
    if (!Check(hr, "D3D11CreateDevice")) return false;

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = (*device)->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(dxgiDevice.Put()));
    if (!Check(hr, "ID3D11Device::QueryInterface(IDXGIDevice)")) return false;

    ComPtr<IInspectable> inspectable;
    hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.Put());
    if (!Check(hr, "CreateDirect3D11DeviceFromDXGIDevice")) return false;

    hr = inspectable->QueryInterface(IID___x_ABI_CWindows_CGraphics_CDirectX_CDirect3D11_CIDirect3DDevice,
                                     reinterpret_cast<void**>(winrtDevice->Put()));
    return Check(hr, "IInspectable::QueryInterface(IDirect3DDevice)") && *winrtDevice;
}

bool CaptureOneFrame(ABI::Windows::Graphics::Capture::IGraphicsCaptureItem* item,
                     ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice* winrtDevice,
                     ID3D11Texture2D** texture,
                     bool diagnostic) {
    ABI::Windows::Graphics::SizeInt32 size = {};
    HRESULT hr = item->get_Size(&size);
    if (!Check(hr, "GraphicsCaptureItem::get_Size")) return false;

    HString poolClass(L"Windows.Graphics.Capture.Direct3D11CaptureFramePool");
    ComPtr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePoolStatics2> poolStatics;
    hr = RoGetActivationFactory(poolClass.Get(),
                                IID___x_ABI_CWindows_CGraphics_CCapture_CIDirect3D11CaptureFramePoolStatics2,
                                reinterpret_cast<void**>(poolStatics.Put()));
    if (!Check(hr, "RoGetActivationFactory(Direct3D11CaptureFramePool)")) return false;

    ComPtr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool> pool;
    hr = poolStatics->CreateFreeThreaded(winrtDevice,
        ABI::Windows::Graphics::DirectX::DirectXPixelFormat_R16G16B16A16Float,
        1,
        size,
        pool.Put());
    if (!Check(hr, "Direct3D11CaptureFramePool::CreateFreeThreaded")) return false;
    WinRtCloseGuard poolClose(pool.Get());

    ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession> session;
    hr = pool->CreateCaptureSession(item, session.Put());
    if (!Check(hr, "Direct3D11CaptureFramePool::CreateCaptureSession")) return false;
    WinRtCloseGuard sessionClose(session.Get());
    DisableCaptureChrome(session.Get());

    Stopwatch frameTimer;
    hr = session->StartCapture();
    if (!Check(hr, "GraphicsCaptureSession::StartCapture")) return false;

    ComPtr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFrame> frame;
    for (int i = 0; i < 120 && !frame; ++i) {
        Sleep(16);
        pool->TryGetNextFrame(frame.Put());
    }
    if (!frame) {
        std::fprintf(stderr, "Timed out waiting for a WGC frame.\n");
        return false;
    }
    if (diagnostic) {
        std::printf("Timing wgc.frame: %.1f ms\n", frameTimer.ElapsedMs());
    }

    ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> surface;
    hr = frame->get_Surface(surface.Put());
    if (!Check(hr, "Direct3D11CaptureFrame::get_Surface")) return false;

    ComPtr<IDirect3DDxgiInterfaceAccess> access;
    hr = surface->QueryInterface(IID_IDirect3DDxgiInterfaceAccess, reinterpret_cast<void**>(access.Put()));
    if (!Check(hr, "IDirect3DSurface::QueryInterface(IDirect3DDxgiInterfaceAccess)")) return false;

    hr = access->GetInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(texture));
    return Check(hr, "IDirect3DDxgiInterfaceAccess::GetInterface(ID3D11Texture2D)") && *texture;
}

}  // namespace

bool CapturePrimaryMonitorFrame(bool diagnostic, NativeCapturedFrame* frame) {
    if (!frame) return false;

    NativeCaptureRuntime runtime;
    if (!InitializePrimaryMonitorRuntime(diagnostic, &runtime)) return false;
    return CaptureFrame(&runtime, diagnostic, frame);
}

bool InitializePrimaryMonitorRuntime(bool diagnostic, NativeCaptureRuntime* runtime) {
    if (!runtime) return false;

    runtime->item.Reset();
    runtime->device.Reset();
    runtime->context.Reset();
    runtime->winrtDevice.Reset();
    runtime->width = 0;
    runtime->height = 0;

    if (!CreatePrimaryMonitorItem(&runtime->item)) return false;

    ABI::Windows::Graphics::SizeInt32 itemSize = {};
    runtime->item->get_Size(&itemSize);
    runtime->width = static_cast<UINT>(itemSize.Width);
    runtime->height = static_cast<UINT>(itemSize.Height);
    if (diagnostic) {
        std::printf("Selected primary monitor item: %d x %d\n", itemSize.Width, itemSize.Height);
    }

    return CreateD3D(&runtime->device, &runtime->context, &runtime->winrtDevice);
}

bool CaptureFrame(NativeCaptureRuntime* runtime, bool diagnostic, NativeCapturedFrame* frame) {
    if (!runtime || !frame || !runtime->item || !runtime->device || !runtime->context || !runtime->winrtDevice) {
        return false;
    }

    frame->device.CopyFrom(runtime->device.Get());
    frame->context.CopyFrom(runtime->context.Get());
    frame->texture.Reset();
    frame->width = 0;
    frame->height = 0;

    if (!CaptureOneFrame(runtime->item.Get(), runtime->winrtDevice.Get(), frame->texture.Put(), diagnostic)) return false;

    D3D11_TEXTURE2D_DESC desc = {};
    frame->texture->GetDesc(&desc);
    frame->width = desc.Width;
    frame->height = desc.Height;
    return true;
}

}  // namespace native_capture
