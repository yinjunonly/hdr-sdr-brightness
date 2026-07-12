#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "night_mode.h"

namespace {

bool g_nightLightActive = false;

std::vector<BYTE> NightLightStateBytes() {
    std::vector<BYTE> data;
    const BYTE marker[] = {0x43, 0x42, 0x01, 0x00};
    data.insert(data.end(), marker, marker + sizeof(marker));
    if (g_nightLightActive) {
        data.push_back(0x10);
        data.push_back(0x00);
    }
    return data;
}

}  // namespace

std::wstring GetCloudSettingsReaderPath() {
    return L"readCloudDataSettings.exe";
}

std::wstring QuotePath(const std::wstring& path) {
    return L"\"" + path + L"\"";
}

bool RunProcessCapture(const std::wstring& commandLine, DWORD, std::string* output) {
    if (commandLine.find(L"bluelightreduction.settings") != std::wstring::npos) {
        *output = "{\"automaticOnSchedule\":true,\"automaticOnSunset\":true}";
        return true;
    }
    return false;
}

bool ReadBinaryValue(HKEY, const wchar_t* keyPath, const wchar_t*, std::vector<BYTE>* data) {
    if (!keyPath || std::wstring(keyPath).find(L"bluelightreductionstate") == std::wstring::npos) {
        return false;
    }
    *data = NightLightStateBytes();
    return true;
}

int main() {
    night_mode::Schedule schedule = {true, 18, 0, 8, 0};

    g_nightLightActive = false;
    night_mode::InvalidateScheduleCache();
    night_mode::Decision beforeSunset = night_mode::Decide(schedule);
    if (beforeSunset.night) {
        std::fprintf(stderr, "FAIL: initial inactive Night Light should be day.\n");
        return 1;
    }

    g_nightLightActive = true;
    night_mode::Decision stale = night_mode::Decide(schedule);
    if (stale.night) {
        std::fprintf(stderr, "FAIL: test did not reproduce the stale active-state cache.\n");
        return 1;
    }

    night_mode::InvalidateActiveStateCache();
    night_mode::Decision refreshed = night_mode::Decide(schedule);
    if (!refreshed.night || refreshed.source != night_mode::DecisionSourceWindowsNightLight) {
        std::fprintf(stderr, "FAIL: active-state refresh did not observe Night Light turning on.\n");
        return 1;
    }

    std::printf("PASS: active Night Light state refreshes without rebuilding schedule caches.\n");
    return 0;
}
