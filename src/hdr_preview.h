#pragma once

#include <windows.h>

struct HdrPreviewRenderer {
    HWND hwnd;
    HMODULE d3d11;
    HMODULE d3dCompiler;
    void* device;
    void* context;
    void* swapChain;
    void* swapChain3;
    void* renderTarget;
    void* vertexShader;
    void* pixelShader;
    int width;
    int height;
    COLORREF shaderBackground;
    bool shaderBackgroundSet;
    bool realHdrColorSpace;
    bool failed;

    HdrPreviewRenderer()
        : hwnd(NULL),
          d3d11(NULL),
          d3dCompiler(NULL),
          device(NULL),
          context(NULL),
          swapChain(NULL),
          swapChain3(NULL),
          renderTarget(NULL),
          vertexShader(NULL),
          pixelShader(NULL),
          width(0),
          height(0),
          shaderBackground(RGB(0, 0, 0)),
          shaderBackgroundSet(false),
          realHdrColorSpace(false),
          failed(false) {}
};

bool HdrPreviewHasSwapChain(const HdrPreviewRenderer& renderer);
void HdrPreviewReleaseDevice(HdrPreviewRenderer* renderer);
void HdrPreviewResetDevice(HdrPreviewRenderer* renderer);
bool HdrPreviewUpdateColorSpace(HdrPreviewRenderer* renderer);
bool HdrPreviewResize(HdrPreviewRenderer* renderer, int width, int height);
bool HdrPreviewEnsureReady(HdrPreviewRenderer* renderer, HWND hwnd, int width, int height,
                           COLORREF background);
bool HdrPreviewRender(HdrPreviewRenderer* renderer);
