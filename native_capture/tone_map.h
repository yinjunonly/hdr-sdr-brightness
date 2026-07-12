#pragma once

#include <d3d11.h>
#include <windows.h>

#include <vector>

namespace native_capture {

struct PixelStats {
    double minR = 0.0;
    double minG = 0.0;
    double minB = 0.0;
    double maxR = 0.0;
    double maxG = 0.0;
    double maxB = 0.0;
    double sumR = 0.0;
    double sumG = 0.0;
    double sumB = 0.0;
    unsigned long long count = 0;

    void Add(double r, double g, double b);
    void Merge(const PixelStats& other);
    void Print(UINT format) const;
};

struct ToneMappedBitmap {
    UINT width = 0;
    UINT height = 0;
    UINT sourceFormat = 0;
    std::vector<BYTE> bgra;
    PixelStats stats;
};

struct ReadbackRegion {
    bool enabled = false;
    LONG x = 0;
    LONG y = 0;
    LONG width = 0;
    LONG height = 0;
};

ToneMappedBitmap ReadbackAndToneMap(ID3D11Device* device,
                                    ID3D11DeviceContext* context,
                                    ID3D11Texture2D* texture,
                                    float sdrWhite,
                                    bool diagnostic,
                                    ReadbackRegion region = {});

}  // namespace native_capture
