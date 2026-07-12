#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>

#include "hdr_preview.h"

namespace {

typedef UINT D3dFeatureLevel;

struct DxgiSampleDesc {
    UINT Count;
    UINT Quality;
};

struct DxgiSwapChainDesc1 {
    UINT Width;
    UINT Height;
    UINT Format;
    BOOL Stereo;
    DxgiSampleDesc SampleDesc;
    UINT BufferUsage;
    UINT BufferCount;
    UINT Scaling;
    UINT SwapEffect;
    UINT AlphaMode;
    UINT Flags;
};

struct D3d11Viewport {
    FLOAT TopLeftX;
    FLOAT TopLeftY;
    FLOAT Width;
    FLOAT Height;
    FLOAT MinDepth;
    FLOAT MaxDepth;
};

const UINT kD3dDriverTypeHardware = 1;
const UINT kD3dDriverTypeWarp = 5;
const UINT kD3d11CreateDeviceBgraSupport = 0x20;
const UINT kD3d11SdkVersion = 7;
const UINT kD3d11PrimitiveTopologyTriangleList = 4;
const D3dFeatureLevel kD3dFeatureLevel11_1 = 0xb100;
const D3dFeatureLevel kD3dFeatureLevel11_0 = 0xb000;
const D3dFeatureLevel kD3dFeatureLevel10_1 = 0xa100;
const D3dFeatureLevel kD3dFeatureLevel10_0 = 0xa000;
const UINT kDxgiFormatUnknown = 0;
const UINT kDxgiFormatR16G16B16A16Float = 10;
const UINT kDxgiUsageRenderTargetOutput = 0x20;
const UINT kDxgiScalingStretch = 0;
const UINT kDxgiSwapEffectFlipDiscard = 4;
const UINT kDxgiAlphaModeIgnore = 3;
const UINT kDxgiMwaNoAltEnter = 0x2;
const UINT kDxgiColorSpaceRgbFullG10NoneP709 = 1;
const UINT kDxgiSwapChainColorSpaceSupportPresent = 0x1;
const UINT kD3dCompileEnableStrictness = 1 << 11;
const UINT kD3dCompileOptimizationLevel3 = 1 << 15;

const GUID kIidIdxgiDevice =
    {0x54ec77fa, 0x1377, 0x44e6, {0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c}};
const GUID kIidIdxgiFactory2 =
    {0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};
const GUID kIidIdxgiSwapChain3 =
    {0x94d99bdb, 0xf1f8, 0x4ab0, {0xb2, 0x36, 0x7d, 0xa0, 0x17, 0x0e, 0xda, 0xb1}};
const GUID kIidD3d11Texture2D =
    {0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c}};

typedef HRESULT(WINAPI* D3D11CreateDeviceFn)(void*, UINT, HMODULE, UINT, const D3dFeatureLevel*,
                                             UINT, UINT, void**, D3dFeatureLevel*, void**);
typedef HRESULT(WINAPI* D3DCompileFn)(LPCVOID, SIZE_T, LPCSTR, const void*, void*, LPCSTR, LPCSTR,
                                      UINT, UINT, void**, void**);

template <typename Fn>
Fn ComMethod(void* object, size_t index) {
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[index]);
}

ULONG ReleaseCom(void* object) {
    if (!object) return 0;
    typedef ULONG(STDMETHODCALLTYPE* ReleaseFn)(void*);
    return ComMethod<ReleaseFn>(object, 2)(object);
}

HRESULT QueryCom(void* object, REFIID iid, void** result) {
    if (!object || !result) return E_POINTER;
    *result = NULL;
    typedef HRESULT(STDMETHODCALLTYPE* QueryInterfaceFn)(void*, REFIID, void**);
    return ComMethod<QueryInterfaceFn>(object, 0)(object, iid, result);
}

void ReleaseHdrPreviewRenderTarget(HdrPreviewRenderer* renderer) {
    if (renderer->renderTarget) {
        ReleaseCom(renderer->renderTarget);
        renderer->renderTarget = NULL;
    }
}

void* BlobBufferPointer(void* blob) {
    typedef LPVOID(STDMETHODCALLTYPE* GetBufferPointerFn)(void*);
    return blob ? ComMethod<GetBufferPointerFn>(blob, 3)(blob) : NULL;
}

SIZE_T BlobBufferSize(void* blob) {
    typedef SIZE_T(STDMETHODCALLTYPE* GetBufferSizeFn)(void*);
    return blob ? ComMethod<GetBufferSizeFn>(blob, 4)(blob) : 0;
}

const char kHdrPreviewVertexShader[] =
    "struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };"
    "VSOut main(uint id : SV_VertexID) {"
    "    float2 uv = float2((id << 1) & 2, id & 2);"
    "    VSOut o;"
    "    o.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);"
    "    o.uv = uv;"
    "    return o;"
    "}";

double SrgbByteToLinear(BYTE value) {
    double c = static_cast<double>(value) / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

std::string BuildHdrPreviewPixelShaderSource(COLORREF background) {
    std::ostringstream ss;
    ss.setf(std::ios::fixed);
    ss.precision(6);
    ss
        << "struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };"
        << "float aaStep(float edge, float value) {"
        << "    float w = max(fwidth(value) * 1.35, 0.0015);"
        << "    return smoothstep(edge - w, edge + w, value);"
        << "}"
        << "float stripe(float y, float center, float width) {"
        << "    float w = max(fwidth(y) * 1.4, 0.0015);"
        << "    return 1.0 - smoothstep(width - w, width + w, abs(y - center));"
        << "}"
        << "float roundedMask(float2 uv) {"
        << "    const float aspect = 246.0 / 74.0;"
        << "    const float radius = 8.0 / 74.0;"
        << "    float2 p = float2((uv.x - 0.5) * aspect, uv.y - 0.5);"
        << "    float2 b = float2(0.5 * aspect, 0.5) - radius;"
        << "    float2 q = abs(p) - b;"
        << "    float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;"
        << "    float w = max(fwidth(dist) * 1.35, 0.0020);"
        << "    return 1.0 - smoothstep(-w, w, dist);"
        << "}"
        << "float4 main(VSOut input) : SV_Target {"
        << "    const float aspect = 246.0 / 74.0;"
        << "    float2 uv = saturate(input.uv);"
        << "    float3 topSky = float3(0.055, 0.42, 1.35);"
        << "    float3 bottomSky = float3(0.48, 1.08, 1.75);"
        << "    float3 color = lerp(topSky, bottomSky, uv.y);"
        << "    float2 sunPos = float2(0.80, 0.27);"
        << "    float2 sunDelta = float2((uv.x - sunPos.x) * aspect, uv.y - sunPos.y);"
        << "    float sunDist = length(sunDelta);"
        << "    float glow = saturate(1.0 - sunDist / 0.36);"
        << "    color += glow * glow * float3(1.8, 1.55, 0.55);"
        << "    float sunDisk = 1.0 - smoothstep(0.125, 0.145, sunDist);"
        << "    color = lerp(color, float3(7.2, 6.1, 2.4), sunDisk);"
        << "    float rearPeak = 0.78 - abs(uv.x - 0.26) * 1.25;"
        << "    float rear = aaStep(rearPeak, uv.y) * aaStep(0.43, uv.y);"
        << "    color = lerp(color, float3(0.055, 0.25, 0.32), rear * 0.92);"
        << "    float frontPeak = 0.88 - abs(uv.x - 0.50) * 1.55;"
        << "    float front = aaStep(frontPeak, uv.y) * aaStep(0.50, uv.y);"
        << "    color = lerp(color, float3(0.045, 0.48, 0.30), front * 0.96);"
        << "    float water = aaStep(0.67, uv.y);"
        << "    float3 waterTop = float3(0.05, 0.95, 1.20);"
        << "    float3 waterBottom = float3(0.03, 0.35, 1.15);"
        << "    float3 waterColor = lerp(waterTop, waterBottom, saturate((uv.y - 0.67) / 0.33));"
        << "    color = lerp(color, waterColor, water);"
        << "    float line1 = stripe(uv.y, 0.76, 0.012) * smoothstep(0.42, 0.58, uv.x) * (1.0 - smoothstep(0.94, 1.0, uv.x));"
        << "    float line2 = stripe(uv.y, 0.88, 0.010) * smoothstep(0.58, 0.72, uv.x) * (1.0 - smoothstep(0.94, 1.0, uv.x));"
        << "    color = lerp(color, float3(3.8, 4.2, 3.3), saturate(line1 + line2));"
        << "    float3 bg = float3("
        << SrgbByteToLinear(GetRValue(background)) << ","
        << SrgbByteToLinear(GetGValue(background)) << ","
        << SrgbByteToLinear(GetBValue(background)) << ");"
        << "    color = lerp(bg, max(color, 0.0), roundedMask(uv));"
        << "    return float4(color, 1.0);"
        << "}";
    return ss.str();
}

bool CompileHdrPreviewShader(HdrPreviewRenderer* renderer, const char* source, const char* entry,
                             const char* target, void** blob) {
    *blob = NULL;
    if (!renderer->d3dCompiler) {
        renderer->d3dCompiler = LoadLibraryW(L"d3dcompiler_47.dll");
        if (!renderer->d3dCompiler) renderer->d3dCompiler = LoadLibraryW(L"d3dcompiler_43.dll");
    }
    if (!renderer->d3dCompiler) return false;

    D3DCompileFn compile = reinterpret_cast<D3DCompileFn>(GetProcAddress(renderer->d3dCompiler, "D3DCompile"));
    if (!compile) {
        FreeLibrary(renderer->d3dCompiler);
        renderer->d3dCompiler = NULL;
        return false;
    }

    void* errors = NULL;
    HRESULT hr = compile(source, std::strlen(source), NULL, NULL, NULL, entry, target,
                         kD3dCompileEnableStrictness | kD3dCompileOptimizationLevel3, 0, blob, &errors);
    if (errors) ReleaseCom(errors);
    return SUCCEEDED(hr) && *blob;
}

bool CreateHdrPreviewShaders(HdrPreviewRenderer* renderer, COLORREF background) {
    void* vsBlob = NULL;
    void* psBlob = NULL;
    if (!CompileHdrPreviewShader(renderer, kHdrPreviewVertexShader, "main", "vs_4_0", &vsBlob)) {
        return false;
    }
    std::string pixelShader = BuildHdrPreviewPixelShaderSource(background);
    if (!CompileHdrPreviewShader(renderer, pixelShader.c_str(), "main", "ps_4_0", &psBlob)) {
        ReleaseCom(vsBlob);
        return false;
    }

    typedef HRESULT(STDMETHODCALLTYPE* CreateShaderFn)(void*, const void*, SIZE_T, void*, void**);
    HRESULT hr = ComMethod<CreateShaderFn>(renderer->device, 12)(
        renderer->device, BlobBufferPointer(vsBlob), BlobBufferSize(vsBlob), NULL, &renderer->vertexShader);
    if (SUCCEEDED(hr)) {
        hr = ComMethod<CreateShaderFn>(renderer->device, 15)(
            renderer->device, BlobBufferPointer(psBlob), BlobBufferSize(psBlob), NULL, &renderer->pixelShader);
    }

    ReleaseCom(vsBlob);
    ReleaseCom(psBlob);
    if (SUCCEEDED(hr) && renderer->vertexShader && renderer->pixelShader) {
        renderer->shaderBackground = background;
        renderer->shaderBackgroundSet = true;
        return true;
    }
    return false;
}

bool EnsureHdrPreviewCore(HdrPreviewRenderer* renderer, COLORREF background) {
    if (renderer->device && renderer->context && renderer->vertexShader && renderer->pixelShader &&
        renderer->shaderBackgroundSet && renderer->shaderBackground == background) {
        return true;
    }
    if (renderer->failed) return false;

    if (renderer->device || renderer->context || renderer->vertexShader || renderer->pixelShader) {
        HdrPreviewReleaseDevice(renderer);
    }

    renderer->d3d11 = LoadLibraryW(L"d3d11.dll");
    if (!renderer->d3d11) {
        renderer->failed = true;
        return false;
    }

    D3D11CreateDeviceFn createDevice =
        reinterpret_cast<D3D11CreateDeviceFn>(GetProcAddress(renderer->d3d11, "D3D11CreateDevice"));
    if (!createDevice) {
        renderer->failed = true;
        HdrPreviewReleaseDevice(renderer);
        return false;
    }

    D3dFeatureLevel levels[] = {
        kD3dFeatureLevel11_1,
        kD3dFeatureLevel11_0,
        kD3dFeatureLevel10_1,
        kD3dFeatureLevel10_0
    };
    D3dFeatureLevel createdLevel = 0;
    UINT flags = kD3d11CreateDeviceBgraSupport;
    HRESULT hr = createDevice(NULL, kD3dDriverTypeHardware, NULL, flags, levels,
                              sizeof(levels) / sizeof(levels[0]), kD3d11SdkVersion,
                              &renderer->device, &createdLevel, &renderer->context);
    if (hr == E_INVALIDARG) {
        hr = createDevice(NULL, kD3dDriverTypeHardware, NULL, flags, levels + 1,
                          sizeof(levels) / sizeof(levels[0]) - 1, kD3d11SdkVersion,
                          &renderer->device, &createdLevel, &renderer->context);
    }
    if (FAILED(hr)) {
        hr = createDevice(NULL, kD3dDriverTypeWarp, NULL, flags, levels + 1,
                          sizeof(levels) / sizeof(levels[0]) - 1, kD3d11SdkVersion,
                          &renderer->device, &createdLevel, &renderer->context);
    }
    if (FAILED(hr) || !renderer->device || !renderer->context || !CreateHdrPreviewShaders(renderer, background)) {
        renderer->failed = true;
        HdrPreviewReleaseDevice(renderer);
        return false;
    }

    return true;
}

bool CreateHdrPreviewRenderTarget(HdrPreviewRenderer* renderer) {
    if (!renderer->swapChain || renderer->renderTarget) return renderer->renderTarget != NULL;

    void* backBuffer = NULL;
    typedef HRESULT(STDMETHODCALLTYPE* GetBufferFn)(void*, UINT, REFIID, void**);
    HRESULT hr = ComMethod<GetBufferFn>(renderer->swapChain, 9)(
        renderer->swapChain, 0, kIidD3d11Texture2D, &backBuffer);
    if (FAILED(hr) || !backBuffer) return false;

    typedef HRESULT(STDMETHODCALLTYPE* CreateRenderTargetViewFn)(void*, void*, const void*, void**);
    hr = ComMethod<CreateRenderTargetViewFn>(renderer->device, 9)(
        renderer->device, backBuffer, NULL, &renderer->renderTarget);
    ReleaseCom(backBuffer);
    return SUCCEEDED(hr) && renderer->renderTarget;
}

bool CreateHdrPreviewSwapChain(HdrPreviewRenderer* renderer, HWND hwnd, int width, int height) {
    if (renderer->swapChain) return true;

    void* dxgiDevice = NULL;
    void* adapter = NULL;
    void* factory = NULL;
    HRESULT hr = QueryCom(renderer->device, kIidIdxgiDevice, &dxgiDevice);
    if (FAILED(hr) || !dxgiDevice) return false;

    typedef HRESULT(STDMETHODCALLTYPE* GetAdapterFn)(void*, void**);
    hr = ComMethod<GetAdapterFn>(dxgiDevice, 7)(dxgiDevice, &adapter);
    if (SUCCEEDED(hr) && adapter) {
        typedef HRESULT(STDMETHODCALLTYPE* GetParentFn)(void*, REFIID, void**);
        hr = ComMethod<GetParentFn>(adapter, 6)(adapter, kIidIdxgiFactory2, &factory);
    }
    if (FAILED(hr) || !factory) {
        if (adapter) ReleaseCom(adapter);
        ReleaseCom(dxgiDevice);
        return false;
    }

    typedef HRESULT(STDMETHODCALLTYPE* MakeWindowAssociationFn)(void*, HWND, UINT);
    ComMethod<MakeWindowAssociationFn>(factory, 8)(factory, hwnd, kDxgiMwaNoAltEnter);

    DxgiSwapChainDesc1 desc = {};
    desc.Width = static_cast<UINT>(std::max(1, width));
    desc.Height = static_cast<UINT>(std::max(1, height));
    desc.Format = kDxgiFormatR16G16B16A16Float;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = kDxgiUsageRenderTargetOutput;
    desc.BufferCount = 2;
    desc.Scaling = kDxgiScalingStretch;
    desc.SwapEffect = kDxgiSwapEffectFlipDiscard;
    desc.AlphaMode = kDxgiAlphaModeIgnore;
    desc.Flags = 0;

    typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainForHwndFn)(void*, void*, HWND, const DxgiSwapChainDesc1*,
                                                                const void*, void*, void**);
    hr = ComMethod<CreateSwapChainForHwndFn>(factory, 15)(
        factory, renderer->device, hwnd, &desc, NULL, NULL, &renderer->swapChain);

    ReleaseCom(factory);
    ReleaseCom(adapter);
    ReleaseCom(dxgiDevice);

    if (FAILED(hr) || !renderer->swapChain) return false;

    renderer->width = width;
    renderer->height = height;
    HdrPreviewUpdateColorSpace(renderer);
    return CreateHdrPreviewRenderTarget(renderer);
}

}

bool HdrPreviewHasSwapChain(const HdrPreviewRenderer& renderer) {
    return renderer.swapChain != NULL;
}

void HdrPreviewReleaseDevice(HdrPreviewRenderer* renderer) {
    ReleaseHdrPreviewRenderTarget(renderer);
    if (renderer->pixelShader) ReleaseCom(renderer->pixelShader);
    if (renderer->vertexShader) ReleaseCom(renderer->vertexShader);
    if (renderer->swapChain3) ReleaseCom(renderer->swapChain3);
    if (renderer->swapChain) ReleaseCom(renderer->swapChain);
    if (renderer->context) ReleaseCom(renderer->context);
    if (renderer->device) ReleaseCom(renderer->device);
    if (renderer->d3dCompiler) FreeLibrary(renderer->d3dCompiler);
    if (renderer->d3d11) FreeLibrary(renderer->d3d11);

    HWND hwnd = renderer->hwnd;
    bool failed = renderer->failed;
    *renderer = HdrPreviewRenderer();
    renderer->hwnd = hwnd;
    renderer->failed = failed;
}

void HdrPreviewResetDevice(HdrPreviewRenderer* renderer) {
    HWND hwnd = renderer->hwnd;
    HdrPreviewReleaseDevice(renderer);
    renderer->hwnd = hwnd;
    renderer->failed = false;
}

bool HdrPreviewUpdateColorSpace(HdrPreviewRenderer* renderer) {
    renderer->realHdrColorSpace = false;
    if (!renderer->swapChain) return false;

    if (!renderer->swapChain3) {
        QueryCom(renderer->swapChain, kIidIdxgiSwapChain3, &renderer->swapChain3);
    }
    if (!renderer->swapChain3) return false;

    typedef HRESULT(STDMETHODCALLTYPE* CheckColorSpaceSupportFn)(void*, UINT, UINT*);
    typedef HRESULT(STDMETHODCALLTYPE* SetColorSpaceFn)(void*, UINT);
    UINT support = 0;
    HRESULT hr = ComMethod<CheckColorSpaceSupportFn>(renderer->swapChain3, 37)(
        renderer->swapChain3, kDxgiColorSpaceRgbFullG10NoneP709, &support);
    if (FAILED(hr) || (support & kDxgiSwapChainColorSpaceSupportPresent) == 0) {
        return false;
    }

    hr = ComMethod<SetColorSpaceFn>(renderer->swapChain3, 38)(
        renderer->swapChain3, kDxgiColorSpaceRgbFullG10NoneP709);
    renderer->realHdrColorSpace = SUCCEEDED(hr);
    return renderer->realHdrColorSpace;
}

bool HdrPreviewResize(HdrPreviewRenderer* renderer, int width, int height) {
    if (!renderer->swapChain) return false;
    if (width == renderer->width && height == renderer->height && renderer->renderTarget) {
        HdrPreviewUpdateColorSpace(renderer);
        return true;
    }

    ReleaseHdrPreviewRenderTarget(renderer);
    typedef HRESULT(STDMETHODCALLTYPE* ResizeBuffersFn)(void*, UINT, UINT, UINT, UINT, UINT);
    HRESULT hr = ComMethod<ResizeBuffersFn>(renderer->swapChain, 13)(
        renderer->swapChain, 0, static_cast<UINT>(std::max(1, width)),
        static_cast<UINT>(std::max(1, height)), kDxgiFormatUnknown, 0);
    if (FAILED(hr)) return false;

    renderer->width = width;
    renderer->height = height;
    HdrPreviewUpdateColorSpace(renderer);
    return CreateHdrPreviewRenderTarget(renderer);
}

bool HdrPreviewEnsureReady(HdrPreviewRenderer* renderer, HWND hwnd, int width, int height,
                           COLORREF background) {
    if (width <= 0 || height <= 0) return false;
    if (!EnsureHdrPreviewCore(renderer, background)) return false;
    if (!CreateHdrPreviewSwapChain(renderer, hwnd, width, height)) {
        renderer->failed = true;
        return false;
    }
    if (!HdrPreviewResize(renderer, width, height)) return false;
    return renderer->realHdrColorSpace && renderer->renderTarget != NULL;
}

bool HdrPreviewRender(HdrPreviewRenderer* renderer) {
    if (!renderer->realHdrColorSpace || !renderer->context || !renderer->renderTarget ||
        !renderer->vertexShader || !renderer->pixelShader || !renderer->swapChain) {
        return false;
    }

    float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    typedef void(STDMETHODCALLTYPE* ClearRenderTargetViewFn)(void*, void*, const float[4]);
    typedef void(STDMETHODCALLTYPE* SetRenderTargetsFn)(void*, UINT, void* const*, void*);
    typedef void(STDMETHODCALLTYPE* SetViewportsFn)(void*, UINT, const D3d11Viewport*);
    typedef void(STDMETHODCALLTYPE* SetPrimitiveTopologyFn)(void*, UINT);
    typedef void(STDMETHODCALLTYPE* SetShaderFn)(void*, void*, void* const*, UINT);
    typedef void(STDMETHODCALLTYPE* DrawFn)(void*, UINT, UINT);
    typedef HRESULT(STDMETHODCALLTYPE* PresentFn)(void*, UINT, UINT);

    ComMethod<ClearRenderTargetViewFn>(renderer->context, 50)(renderer->context, renderer->renderTarget, clear);

    void* targets[1] = {renderer->renderTarget};
    ComMethod<SetRenderTargetsFn>(renderer->context, 33)(renderer->context, 1, targets, NULL);

    D3d11Viewport viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<FLOAT>(std::max(1, renderer->width));
    viewport.Height = static_cast<FLOAT>(std::max(1, renderer->height));
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    ComMethod<SetViewportsFn>(renderer->context, 44)(renderer->context, 1, &viewport);
    ComMethod<SetPrimitiveTopologyFn>(renderer->context, 24)(renderer->context, kD3d11PrimitiveTopologyTriangleList);
    ComMethod<SetShaderFn>(renderer->context, 11)(renderer->context, renderer->vertexShader, NULL, 0);
    ComMethod<SetShaderFn>(renderer->context, 9)(renderer->context, renderer->pixelShader, NULL, 0);
    ComMethod<DrawFn>(renderer->context, 13)(renderer->context, 3, 0);

    HRESULT hr = ComMethod<PresentFn>(renderer->swapChain, 8)(renderer->swapChain, 1, 0);
    return SUCCEEDED(hr);
}
