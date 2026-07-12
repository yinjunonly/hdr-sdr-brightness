#include "capture_bitmap.h"

#include <algorithm>
#include <cstring>

namespace native_capture {

ToneMappedBitmap CropToneMappedBitmap(const ToneMappedBitmap& source, const RECT& region) {
    ToneMappedBitmap result;
    result.sourceFormat = source.sourceFormat;
    if (source.width == 0 || source.height == 0 || source.bgra.empty()) return result;

    LONG left = std::clamp(region.left, 0L, static_cast<LONG>(source.width));
    LONG top = std::clamp(region.top, 0L, static_cast<LONG>(source.height));
    LONG right = std::clamp(region.right, 0L, static_cast<LONG>(source.width));
    LONG bottom = std::clamp(region.bottom, 0L, static_cast<LONG>(source.height));
    if (right <= left || bottom <= top) return result;

    result.width = static_cast<UINT>(right - left);
    result.height = static_cast<UINT>(bottom - top);
    size_t rowBytes = static_cast<size_t>(result.width) * 4;
    result.bgra.resize(rowBytes * result.height);
    size_t sourceRowBytes = static_cast<size_t>(source.width) * 4;
    for (UINT y = 0; y < result.height; ++y) {
        const BYTE* sourceRow = source.bgra.data() + (static_cast<size_t>(top) + y) * sourceRowBytes +
                                static_cast<size_t>(left) * 4;
        std::memcpy(result.bgra.data() + static_cast<size_t>(y) * rowBytes, sourceRow, rowBytes);
    }
    return result;
}

}  // namespace native_capture
