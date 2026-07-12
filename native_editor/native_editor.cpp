#include "bmp_codec.h"
#include "editor_options.h"
#include "editor_single_instance.h"
#include "editor_window.h"
#include "wic_png.h"

#include <windows.h>
#include <objbase.h>
#include <objidl.h>
#include <gdiplus.h>

#include <string>
#include <utility>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    editor::EditorOptions options;
    std::wstring error;
    if (!editor::ParseEditorOptions(&options, &error)) {
        MessageBoxW(nullptr, error.c_str(), L"HDR SDR Native Editor", MB_OK | MB_ICONERROR);
        return 2;
    }

    editor::EditorSingleInstance singleInstance;
    if (!options.warmup && !singleInstance.AcquireLatest()) return 0;

    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) {
        MessageBoxW(nullptr, L"Could not initialize Windows imaging.",
                    L"HDR SDR Native Editor", MB_OK | MB_ICONERROR);
        return 8;
    }

    Gdiplus::GdiplusStartupInput startupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::Status gdiplus = Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr);
    if (gdiplus != Gdiplus::Ok) {
        if (SUCCEEDED(com)) CoUninitialize();
        MessageBoxW(nullptr, L"Could not initialize native drawing.",
                    L"HDR SDR Native Editor", MB_OK | MB_ICONERROR);
        return 8;
    }

    if (options.warmup) {
        editor::BgraImage image;
        image.width = 16;
        image.height = 16;
        image.pixels.assign(16 * 16 * 4, 255);
        editor::ImageDocument document(std::move(image));
        editor::EditOperation marker;
        marker.type = editor::EditOperationType::Marker;
        marker.rect = RECT{2, 2, 14, 14};
        marker.strokeWidth = 2;
        document.AddOperation(marker);
        editor::BgraImage rendered = document.Render();
        std::vector<BYTE> png;
        editor::EncodePng(rendered, &png, &error);
        Gdiplus::GdiplusShutdown(gdiplusToken);
        if (SUCCEEDED(com)) CoUninitialize();
        return 0;
    }

    editor::BgraImage image;
    if (!editor::LoadBmp(options.imagePath, &image, &error)) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        if (SUCCEEDED(com)) CoUninitialize();
        MessageBoxW(nullptr, error.c_str(), L"HDR SDR Native Editor", MB_OK | MB_ICONERROR);
        return 8;
    }

    int result = editor::ShowEditorWindow(instance, options, std::move(image));
    Gdiplus::GdiplusShutdown(gdiplusToken);
    if (SUCCEEDED(com)) CoUninitialize();
    return result;
}
