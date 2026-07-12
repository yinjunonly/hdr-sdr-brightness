#include "tone_map.h"

#include "native_common.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <thread>

namespace native_capture {

namespace {

class ToneMapLookup {
public:
    explicit ToneMapLookup(float sdrWhite)
        : sdrWhite_(std::clamp(sdrWhite, 0.1f, 10.0f)) {
        for (size_t i = 0; i < srgb_.size(); ++i) {
            srgb_[i] = ToByte(LinearToSrgb(i / static_cast<float>(srgb_.size() - 1)));
        }

        for (size_t i = 0; i < luminance_.size(); ++i) {
            float value = i * maxLuminance_ / static_cast<float>(luminance_.size() - 1);
            luminance_[i] = ToneMapDesktop(value);
        }
    }

    BYTE ToSrgbByte(float value) const {
        if (value <= 0.0f) return 0;
        if (value >= 1.0f) return 255;
        int index = static_cast<int>(value * static_cast<float>(srgb_.size() - 1) + 0.5f);
        return srgb_[std::clamp(index, 0, static_cast<int>(srgb_.size() - 1))];
    }

    float MapLuminance(float value) const {
        if (value <= 0.0f) return 0.0f;
        if (value >= maxLuminance_) return luminance_.back();
        int index = static_cast<int>(value * static_cast<float>(luminance_.size() - 1) / maxLuminance_ + 0.5f);
        return luminance_[std::clamp(index, 0, static_cast<int>(luminance_.size() - 1))];
    }

    float Exposure() const { return exposure_; }

private:
    static BYTE ToByte(float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        return static_cast<BYTE>(value * 255.0f + 0.5f);
    }

    static float LinearToSrgb(float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        return value <= 0.0031308f
                   ? value * 12.92f
                   : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    }

    float ToneMapDesktop(float value) const {
        constexpr float sdrOutputWhite = 1.0f;
        constexpr float hdrKnee = 0.55f;
        constexpr float hdrShoulder = 5.0f;

        float kneeStart = std::clamp(sdrWhite_ * hdrKnee, 0.0f, sdrWhite_);
        float kneeOutput = kneeStart / sdrWhite_ * sdrOutputWhite;
        if (value <= kneeStart) {
            return std::clamp(value / sdrWhite_ * sdrOutputWhite, 0.0f, 1.0f);
        }

        float over = (value - kneeStart) / hdrShoulder;
        float highlight = 1.0f - std::exp(-std::max(0.0f, over));
        return std::clamp(kneeOutput + (1.0f - kneeOutput) * highlight, 0.0f, 1.0f);
    }

    static constexpr float maxLuminance_ = 16.0f;
    static constexpr float exposure_ = 0.75f;
    float sdrWhite_;
    std::array<BYTE, 4096> srgb_{};
    std::array<float, 8192> luminance_{};
};

float HalfToFloat(USHORT value) {
    UINT sign = (value >> 15) & 0x1;
    UINT exponent = (value >> 10) & 0x1f;
    UINT mantissa = value & 0x3ff;

    UINT bits = 0;
    if (exponent == 0) {
        if (mantissa == 0) {
            bits = sign << 31;
        } else {
            exponent = 1;
            while ((mantissa & 0x400) == 0) {
                mantissa <<= 1;
                --exponent;
            }
            mantissa &= 0x3ff;
            UINT floatExponent = exponent + (127 - 15);
            bits = (sign << 31) | (floatExponent << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) {
        bits = (sign << 31) | 0x7f800000 | (mantissa << 13);
    } else {
        UINT floatExponent = exponent + (127 - 15);
        bits = (sign << 31) | (floatExponent << 23) | (mantissa << 13);
    }

    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

std::array<float, 65536> BuildHalfFloatLookup() {
    std::array<float, 65536> lookup = {};
    for (size_t i = 0; i < lookup.size(); ++i) {
        lookup[i] = HalfToFloat(static_cast<USHORT>(i));
    }
    return lookup;
}

const std::array<float, 65536>& HalfFloatLookup() {
    static const std::array<float, 65536> lookup = BuildHalfFloatLookup();
    return lookup;
}

UINT ChooseWorkerCount(UINT height) {
    if (height < 256) return 1;
    UINT hardware = std::thread::hardware_concurrency();
    if (hardware == 0) hardware = 4;
    return std::clamp<UINT>(hardware, 1, std::min<UINT>(height, 16));
}

bool ResolveReadbackRegion(const D3D11_TEXTURE2D_DESC& sourceDesc,
                           ReadbackRegion region,
                           D3D11_BOX* sourceBox,
                           UINT* width,
                           UINT* height) {
    if (!sourceBox || !width || !height) return false;

    if (!region.enabled) {
        *sourceBox = D3D11_BOX{0, 0, 0, sourceDesc.Width, sourceDesc.Height, 1};
        *width = sourceDesc.Width;
        *height = sourceDesc.Height;
        return true;
    }

    if (region.width <= 0 || region.height <= 0) return false;

    LONG sourceWidth = static_cast<LONG>(sourceDesc.Width);
    LONG sourceHeight = static_cast<LONG>(sourceDesc.Height);
    LONG left = std::clamp(region.x, 0L, sourceWidth);
    LONG top = std::clamp(region.y, 0L, sourceHeight);
    LONG right = std::clamp(region.x + region.width, 0L, sourceWidth);
    LONG bottom = std::clamp(region.y + region.height, 0L, sourceHeight);
    if (right <= left || bottom <= top) return false;

    *sourceBox = D3D11_BOX{
        static_cast<UINT>(left),
        static_cast<UINT>(top),
        0,
        static_cast<UINT>(right),
        static_cast<UINT>(bottom),
        1
    };
    *width = static_cast<UINT>(right - left);
    *height = static_cast<UINT>(bottom - top);
    return true;
}

}  // namespace

void PixelStats::Add(double r, double g, double b) {
    if (count == 0) {
        minR = maxR = r;
        minG = maxG = g;
        minB = maxB = b;
    } else {
        minR = std::min(minR, r);
        minG = std::min(minG, g);
        minB = std::min(minB, b);
        maxR = std::max(maxR, r);
        maxG = std::max(maxG, g);
        maxB = std::max(maxB, b);
    }

    sumR += r;
    sumG += g;
    sumB += b;
    ++count;
}

void PixelStats::Merge(const PixelStats& other) {
    if (other.count == 0) return;
    if (count == 0) {
        *this = other;
        return;
    }

    minR = std::min(minR, other.minR);
    minG = std::min(minG, other.minG);
    minB = std::min(minB, other.minB);
    maxR = std::max(maxR, other.maxR);
    maxG = std::max(maxG, other.maxG);
    maxB = std::max(maxB, other.maxB);
    sumR += other.sumR;
    sumG += other.sumG;
    sumB += other.sumB;
    count += other.count;
}

void PixelStats::Print(UINT format) const {
    double divisor = count ? static_cast<double>(count) : 1.0;
    std::printf("Sample R min/avg/max: %.6f / %.6f / %.6f\n", minR, sumR / divisor, maxR);
    std::printf("Sample G min/avg/max: %.6f / %.6f / %.6f\n", minG, sumG / divisor, maxG);
    std::printf("Sample B min/avg/max: %.6f / %.6f / %.6f\n", minB, sumB / divisor, maxB);
    if (format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
        std::printf((maxR > 1.0 || maxG > 1.0 || maxB > 1.0)
                        ? "HDR signal: sampled float values exceed 1.0.\n"
                        : "No sampled HDR headroom: float values did not exceed 1.0.\n");
    }
}

ToneMappedBitmap ReadbackAndToneMap(ID3D11Device* device,
                                    ID3D11DeviceContext* context,
                                    ID3D11Texture2D* texture,
                                    float sdrWhite,
                                    bool diagnostic,
                                    ReadbackRegion region) {
    ToneMappedBitmap result;

    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    result.sourceFormat = desc.Format;

    D3D11_BOX sourceBox = {};
    UINT readbackWidth = 0;
    UINT readbackHeight = 0;
    if (!ResolveReadbackRegion(desc, region, &sourceBox, &readbackWidth, &readbackHeight)) {
        std::fprintf(stderr, "Selected region is outside the captured frame.\n");
        return {};
    }
    result.width = readbackWidth;
    result.height = readbackHeight;

    if (diagnostic) {
        std::printf("Native texture desc: %u x %u, format %u\n", desc.Width, desc.Height, desc.Format);
        if (region.enabled) {
            std::printf("Readback region: %ld,%ld %ldx%ld -> %u x %u\n",
                        region.x,
                        region.y,
                        region.width,
                        region.height,
                        readbackWidth,
                        readbackHeight);
        }
    }

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Width = readbackWidth;
    stagingDesc.Height = readbackHeight;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    Stopwatch readbackTimer;
    ComPtr<ID3D11Texture2D> staging;
    HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, staging.Put());
    if (!Check(hr, "ID3D11Device::CreateTexture2D(staging)")) return {};
    context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, texture, 0, &sourceBox);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (!Check(hr, "ID3D11DeviceContext::Map")) return {};

    result.bgra.resize(static_cast<size_t>(readbackWidth) * readbackHeight * 4);
    ToneMapLookup toneMap(sdrWhite);
    UINT stepX = std::max<UINT>(1, readbackWidth / 256);
    UINT stepY = std::max<UINT>(1, readbackHeight / 144);
    UINT workerCount = ChooseWorkerCount(readbackHeight);
    std::vector<PixelStats> workerStats(workerCount);
    std::vector<std::thread> workers;
    workers.reserve(workerCount > 0 ? workerCount - 1 : 0);

    const BYTE* sourceBase = static_cast<const BYTE*>(mapped.pData);
    const std::array<float, 65536>& halfLookup = HalfFloatLookup();
    auto processRows = [&](UINT workerIndex, UINT yBegin, UINT yEnd) {
        PixelStats localStats;
        for (UINT y = yBegin; y < yEnd; ++y) {
            const BYTE* sourceRow = sourceBase + mapped.RowPitch * y;
            BYTE* targetRow = result.bgra.data() + static_cast<size_t>(readbackWidth) * y * 4;
            for (UINT x = 0; x < readbackWidth; ++x) {
                float r = 0.0f, g = 0.0f, b = 0.0f;
                if (desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
                    const USHORT* source = reinterpret_cast<const USHORT*>(sourceRow + x * 8);
                    r = halfLookup[source[0]];
                    g = halfLookup[source[1]];
                    b = halfLookup[source[2]];
                } else if (desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                           desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
                    const BYTE* source = sourceRow + x * 4;
                    b = source[0] / 255.0f;
                    g = source[1] / 255.0f;
                    r = source[2] / 255.0f;
                }

                if (diagnostic && y % stepY == 0 && x % stepX == 0) {
                    localStats.Add(r, g, b);
                }

                r = std::max(0.0f, r * toneMap.Exposure());
                g = std::max(0.0f, g * toneMap.Exposure());
                b = std::max(0.0f, b * toneMap.Exposure());
                float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;

                BYTE* target = targetRow + x * 4;
                if (luminance <= 0.000001f) {
                    target[0] = target[1] = target[2] = 0;
                } else {
                    float mappedLuminance = toneMap.MapLuminance(luminance);
                    float scale = mappedLuminance / luminance;
                    target[2] = toneMap.ToSrgbByte(r * scale);
                    target[1] = toneMap.ToSrgbByte(g * scale);
                    target[0] = toneMap.ToSrgbByte(b * scale);
                }
                target[3] = 255;
            }
        }
        workerStats[workerIndex] = localStats;
    };

    UINT rowsPerWorker = (readbackHeight + workerCount - 1) / workerCount;
    for (UINT worker = 1; worker < workerCount; ++worker) {
        UINT yBegin = std::min(readbackHeight, worker * rowsPerWorker);
        UINT yEnd = std::min(readbackHeight, yBegin + rowsPerWorker);
        workers.emplace_back(processRows, worker, yBegin, yEnd);
    }
    processRows(0, 0, std::min(readbackHeight, rowsPerWorker));
    for (std::thread& worker : workers) {
        worker.join();
    }

    if (diagnostic) {
        for (const PixelStats& stats : workerStats) {
            result.stats.Merge(stats);
        }
    }

    context->Unmap(staging.Get(), 0);
    if (diagnostic) {
        std::printf("Timing native.readback_tone_map: %.1f ms\n", readbackTimer.ElapsedMs());
    }
    return result;
}

}  // namespace native_capture
