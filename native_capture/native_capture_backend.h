#pragma once

#include "native_common.h"

#include <d3d11.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.directx.direct3d11.h>

namespace native_capture {

struct NativeCaptureRuntime {
    ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem> item;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice> winrtDevice;
    UINT width = 0;
    UINT height = 0;
};

struct NativeCapturedFrame {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> texture;
    UINT width = 0;
    UINT height = 0;
};

bool InitializePrimaryMonitorRuntime(bool diagnostic, NativeCaptureRuntime* runtime);
bool CaptureFrame(NativeCaptureRuntime* runtime, bool diagnostic, NativeCapturedFrame* frame);
bool CapturePrimaryMonitorFrame(bool diagnostic, NativeCapturedFrame* frame);

}  // namespace native_capture
