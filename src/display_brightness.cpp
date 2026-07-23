#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <vector>

#include "display_brightness.h"

namespace {

const UINT32 kQdcOnlyActivePaths = 0x00000002;
const UINT32 kDisplayConfigPathActive = 0x00000001;
const UINT32 kDisplayConfigGetAdvancedColorInfo = 9;
const UINT32 kDisplayConfigGetSdrWhiteLevel = 11;
const UINT32 kDisplayConfigSetSdrWhiteLevel = 0xFFFFFFEEu;

struct AppDisplayConfigPathSourceInfo {
    LUID adapterId;
    UINT32 id;
    UINT32 modeInfoIdx;
    UINT32 statusFlags;
};

struct AppDisplayConfigRational {
    UINT32 Numerator;
    UINT32 Denominator;
};

struct AppDisplayConfigPathTargetInfo {
    LUID adapterId;
    UINT32 id;
    UINT32 modeInfoIdx;
    UINT32 outputTechnology;
    UINT32 rotation;
    UINT32 scaling;
    AppDisplayConfigRational refreshRate;
    UINT32 scanLineOrdering;
    BOOL targetAvailable;
    UINT32 statusFlags;
};

struct AppDisplayConfigPathInfo {
    AppDisplayConfigPathSourceInfo sourceInfo;
    AppDisplayConfigPathTargetInfo targetInfo;
    UINT32 flags;
};

struct AppDisplayConfigModeInfo {
    UINT32 infoType;
    UINT32 id;
    LUID adapterId;
    BYTE modeInfo[48];
};

struct AppDisplayConfigDeviceInfoHeader {
    UINT32 type;
    UINT32 size;
    LUID adapterId;
    UINT32 id;
};

struct AppDisplayConfigGetAdvancedColorInfo {
    AppDisplayConfigDeviceInfoHeader header;
    UINT32 value;
    UINT32 colorEncoding;
    UINT32 bitsPerColorChannel;
};

struct AppDisplayConfigSdrWhiteLevel {
    AppDisplayConfigDeviceInfoHeader header;
    ULONG SDRWhiteLevel;
};

struct AppDisplayConfigSetSdrWhiteLevel {
    AppDisplayConfigDeviceInfoHeader header;
    UINT32 SDRWhiteLevel;
    BYTE finalValue;
};

typedef LONG(WINAPI* GetDisplayConfigBufferSizesFn)(UINT32, UINT32*, UINT32*);
typedef LONG(WINAPI* QueryDisplayConfigFn)(UINT32, UINT32*, AppDisplayConfigPathInfo*, UINT32*,
                                           AppDisplayConfigModeInfo*, UINT32*);
typedef LONG(WINAPI* DisplayConfigGetDeviceInfoFn)(AppDisplayConfigDeviceInfoHeader*);
typedef LONG(WINAPI* DisplayConfigSetDeviceInfoFn)(AppDisplayConfigDeviceInfoHeader*);
typedef HRESULT(WINAPI* DwmpSdrToHdrBoostFn)(HMONITOR, double);

struct DisplayConfigApi {
    GetDisplayConfigBufferSizesFn getBufferSizes;
    QueryDisplayConfigFn query;
    DisplayConfigGetDeviceInfoFn getDeviceInfo;
    DisplayConfigSetDeviceInfoFn setDeviceInfo;
};

struct DwmFallbackContext {
    DwmpSdrToHdrBoostFn fn;
    double boost;
    int successCount;
};

static_assert(sizeof(AppDisplayConfigPathSourceInfo) == 20, "Unexpected DISPLAYCONFIG source size");
static_assert(sizeof(AppDisplayConfigPathTargetInfo) == 48, "Unexpected DISPLAYCONFIG target size");
static_assert(sizeof(AppDisplayConfigPathInfo) == 72, "Unexpected DISPLAYCONFIG path size");
static_assert(sizeof(AppDisplayConfigModeInfo) == 64, "Unexpected DISPLAYCONFIG mode size");
static_assert(sizeof(AppDisplayConfigDeviceInfoHeader) == 20, "Unexpected DISPLAYCONFIG header size");
static_assert(sizeof(AppDisplayConfigGetAdvancedColorInfo) == 32, "Unexpected advanced color size");
static_assert(sizeof(AppDisplayConfigSdrWhiteLevel) == 24, "Unexpected SDR white level size");

int ClampPercent(int value) {
    return std::max(0, std::min(100, value));
}

bool LoadDisplayConfigApi(DisplayConfigApi* api) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) user32 = LoadLibraryW(L"user32.dll");
    if (!user32) return false;

    api->getBufferSizes = reinterpret_cast<GetDisplayConfigBufferSizesFn>(
        GetProcAddress(user32, "GetDisplayConfigBufferSizes"));
    api->query = reinterpret_cast<QueryDisplayConfigFn>(GetProcAddress(user32, "QueryDisplayConfig"));
    api->getDeviceInfo = reinterpret_cast<DisplayConfigGetDeviceInfoFn>(
        GetProcAddress(user32, "DisplayConfigGetDeviceInfo"));
    api->setDeviceInfo = reinterpret_cast<DisplayConfigSetDeviceInfoFn>(
        GetProcAddress(user32, "DisplayConfigSetDeviceInfo"));

    return api->getBufferSizes && api->query && api->getDeviceInfo && api->setDeviceInfo;
}

bool QueryActivePaths(DisplayConfigApi* api, std::vector<AppDisplayConfigPathInfo>* paths) {
    UINT32 pathCount = 0;
    UINT32 modeCount = 0;
    LONG rc = api->getBufferSizes(kQdcOnlyActivePaths, &pathCount, &modeCount);
    if (rc != ERROR_SUCCESS || pathCount == 0) return false;

    for (int attempt = 0; attempt < 3; ++attempt) {
        std::vector<AppDisplayConfigPathInfo> pathBuffer(pathCount);
        std::vector<AppDisplayConfigModeInfo> modeBuffer(modeCount);
        UINT32 queryPathCount = pathCount;
        UINT32 queryModeCount = modeCount;
        rc = api->query(kQdcOnlyActivePaths, &queryPathCount, pathBuffer.data(), &queryModeCount,
                        modeBuffer.data(), NULL);
        if (rc == ERROR_SUCCESS) {
            pathBuffer.resize(queryPathCount);
            paths->swap(pathBuffer);
            return true;
        }
        if (rc != ERROR_INSUFFICIENT_BUFFER) return false;
        rc = api->getBufferSizes(kQdcOnlyActivePaths, &pathCount, &modeCount);
        if (rc != ERROR_SUCCESS) return false;
    }

    return false;
}

bool IsHdrEnabled(DisplayConfigApi* api, const AppDisplayConfigPathInfo& path) {
    AppDisplayConfigGetAdvancedColorInfo info = {};
    info.header.type = kDisplayConfigGetAdvancedColorInfo;
    info.header.size = sizeof(info);
    info.header.adapterId = path.targetInfo.adapterId;
    info.header.id = path.targetInfo.id;

    LONG rc = api->getDeviceInfo(&info.header);
    if (rc != ERROR_SUCCESS) return false;
    return (info.value & 0x2u) != 0;
}

bool GetSdrWhiteLevel(DisplayConfigApi* api, const AppDisplayConfigPathInfo& path, UINT32* level) {
    AppDisplayConfigSdrWhiteLevel info = {};
    info.header.type = kDisplayConfigGetSdrWhiteLevel;
    info.header.size = sizeof(info);
    info.header.adapterId = path.targetInfo.adapterId;
    info.header.id = path.targetInfo.id;

    LONG rc = api->getDeviceInfo(&info.header);
    if (rc != ERROR_SUCCESS) return false;
    *level = info.SDRWhiteLevel;
    return true;
}

LONG SetSdrWhiteLevel(DisplayConfigApi* api, const AppDisplayConfigPathInfo& path, UINT32 level) {
    AppDisplayConfigSetSdrWhiteLevel info = {};
    info.header.type = kDisplayConfigSetSdrWhiteLevel;
    info.header.size = sizeof(info);
    info.header.adapterId = path.targetInfo.adapterId;
    info.header.id = path.targetInfo.id;
    info.SDRWhiteLevel = level;
    info.finalValue = 1;

    return api->setDeviceInfo(&info.header);
}

BOOL CALLBACK ApplyDwmFallbackToMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM param) {
    DwmFallbackContext* context = reinterpret_cast<DwmFallbackContext*>(param);
    HRESULT hr = context->fn(monitor, context->boost);
    if (SUCCEEDED(hr)) ++context->successCount;
    return TRUE;
}

bool ApplyDwmFallback(UINT32 sdrLevel, int* successCount) {
    HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
    if (!dwmapi) return false;

    DwmpSdrToHdrBoostFn fn =
        reinterpret_cast<DwmpSdrToHdrBoostFn>(GetProcAddress(dwmapi, MAKEINTRESOURCEA(171)));
    if (!fn) {
        FreeLibrary(dwmapi);
        return false;
    }

    DwmFallbackContext context = {};
    context.fn = fn;
    context.boost = static_cast<double>(sdrLevel) / 1000.0;
    context.successCount = 0;
    EnumDisplayMonitors(NULL, NULL, ApplyDwmFallbackToMonitor, reinterpret_cast<LPARAM>(&context));
    FreeLibrary(dwmapi);

    if (successCount) *successCount = context.successCount;
    return context.successCount > 0;
}

UINT32 MoveLevelToward(UINT32 current, UINT32 target, UINT32 step) {
    if (current == target) return target;
    if (current < target) {
        UINT32 delta = target - current;
        return delta <= step ? target : current + step;
    }
    UINT32 delta = current - target;
    return delta <= step ? target : current - step;
}

}

UINT32 BrightnessPercentToSdrLevel(int brightness) {
    brightness = ClampPercent(brightness);
    return 1000u + static_cast<UINT32>(brightness) * 50u;
}

int SdrWhiteLevelToBrightnessPercent(UINT32 level) {
    if (level <= 1000u) return 0;
    if (level >= 6000u) return 100;
    return static_cast<int>((level - 1000u + 25u) / 50u);
}

bool ReadCurrentSdrBrightness(int* brightness) {
    if (!brightness) return false;

    DisplayConfigApi api = {};
    if (!LoadDisplayConfigApi(&api)) return false;

    std::vector<AppDisplayConfigPathInfo> paths;
    if (!QueryActivePaths(&api, &paths)) return false;

    bool found = false;
    int sharedBrightness = 0;
    for (size_t i = 0; i < paths.size(); ++i) {
        const AppDisplayConfigPathInfo& path = paths[i];
        if ((path.flags & kDisplayConfigPathActive) == 0 || !path.targetInfo.targetAvailable) continue;
        if (!IsHdrEnabled(&api, path)) continue;

        UINT32 level = 0;
        if (!GetSdrWhiteLevel(&api, path, &level)) return false;

        int current = SdrWhiteLevelToBrightnessPercent(level);
        if (!found) {
            sharedBrightness = current;
            found = true;
        } else if (current != sharedBrightness) {
            return false;
        }
    }

    if (!found) return false;
    *brightness = sharedBrightness;
    return true;
}

ApplyResult ApplySdrLevelStep(UINT32 targetLevel, bool smooth, UINT32 fallbackCurrentLevel,
                              UINT32 transitionStepLevel) {
    ApplyResult result;
    result.complete = true;

    DisplayConfigApi api = {};
    if (LoadDisplayConfigApi(&api)) {
        std::vector<AppDisplayConfigPathInfo> paths;
        if (QueryActivePaths(&api, &paths)) {
            for (size_t i = 0; i < paths.size(); ++i) {
                const AppDisplayConfigPathInfo& path = paths[i];
                if ((path.flags & kDisplayConfigPathActive) == 0 || !path.targetInfo.targetAvailable) continue;
                if (!IsHdrEnabled(&api, path)) continue;

                ++result.targetCount;

                UINT32 currentLevel = 0;
                bool hasCurrent = GetSdrWhiteLevel(&api, path, &currentLevel);
                if (hasCurrent && currentLevel == targetLevel) {
                    ++result.successCount;
                    continue;
                }

                result.complete = false;
                UINT32 nextLevel = targetLevel;
                if (smooth && hasCurrent) {
                    nextLevel = MoveLevelToward(currentLevel, targetLevel, transitionStepLevel);
                }

                LONG rc = SetSdrWhiteLevel(&api, path, nextLevel);
                result.lastError = rc;
                if (rc == ERROR_SUCCESS) {
                    ++result.successCount;
                    result.changed = true;
                    result.appliedLevel = nextLevel;
                }
            }
        }
    }

    result.ok = result.targetCount > 0 && result.successCount == result.targetCount;
    if (!result.ok) {
        UINT32 fallbackLevel = targetLevel;
        result.complete = true;
        if (smooth && fallbackCurrentLevel != 0) {
            fallbackLevel = MoveLevelToward(fallbackCurrentLevel, targetLevel, transitionStepLevel);
            result.complete = fallbackLevel == targetLevel;
        }

        int dwmSuccess = 0;
        if (ApplyDwmFallback(fallbackLevel, &dwmSuccess)) {
            result.ok = true;
            result.usedDwmFallback = true;
            result.successCount += dwmSuccess;
            result.changed = true;
            result.appliedLevel = fallbackLevel;
        }
    }

    return result;
}

ApplyResult CheckSdrBrightness(int brightness) {
    ApplyResult result;
    UINT32 targetLevel = BrightnessPercentToSdrLevel(brightness);
    result.complete = true;

    DisplayConfigApi api = {};
    if (!LoadDisplayConfigApi(&api)) return result;

    std::vector<AppDisplayConfigPathInfo> paths;
    if (!QueryActivePaths(&api, &paths)) return result;

    for (size_t i = 0; i < paths.size(); ++i) {
        const AppDisplayConfigPathInfo& path = paths[i];
        if ((path.flags & kDisplayConfigPathActive) == 0 || !path.targetInfo.targetAvailable) continue;
        if (!IsHdrEnabled(&api, path)) continue;

        ++result.targetCount;
        UINT32 currentLevel = 0;
        if (GetSdrWhiteLevel(&api, path, &currentLevel)) {
            if (currentLevel == targetLevel) {
                ++result.successCount;
            } else {
                result.complete = false;
            }
        } else {
            result.complete = false;
        }
    }

    result.ok = result.targetCount > 0 && result.successCount == result.targetCount && result.complete;
    return result;
}
