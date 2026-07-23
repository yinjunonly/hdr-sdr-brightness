#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601

#include "night_mode.h"

#include <windows.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "process_util.h"
#include "registry_util.h"

namespace night_mode {
namespace {

struct MaybeBool {
    bool known;
    bool value;

    MaybeBool() : known(false), value(false) {}
    MaybeBool(bool knownValue, bool boolValue) : known(knownValue), value(boolValue) {}
};

const wchar_t* const kNightLightStateKeys[] = {
    L"Software\\Microsoft\\Windows\\CurrentVersion\\CloudStore\\Store\\DefaultAccount\\Current\\default$windows.data.bluelightreduction.bluelightreductionstate\\windows.data.bluelightreduction.bluelightreductionstate",
    L"Software\\Microsoft\\Windows\\CurrentVersion\\CloudStore\\Store\\DefaultAccount\\Cloud\\default$windows.data.bluelightreduction.bluelightreductionstate\\windows.data.bluelightreduction.bluelightreductionstate",
    L"Software\\Microsoft\\Windows\\CurrentVersion\\CloudStore\\Store\\Cache\\DefaultAccount\\$$windows.data.bluelightreduction.bluelightreductionstate\\Current"
};

const wchar_t* const kNightLightSettingsKeys[] = {
    L"Software\\Microsoft\\Windows\\CurrentVersion\\CloudStore\\Store\\DefaultAccount\\Current\\default$windows.data.bluelightreduction.settings\\windows.data.bluelightreduction.settings",
    L"Software\\Microsoft\\Windows\\CurrentVersion\\CloudStore\\Store\\DefaultAccount\\Cloud\\default$windows.data.bluelightreduction.settings\\windows.data.bluelightreduction.settings",
    L"Software\\Microsoft\\Windows\\CurrentVersion\\CloudStore\\Store\\Cache\\DefaultAccount\\$$windows.data.bluelightreduction.settings\\Current"
};

MaybeBool g_cachedSunsetSchedule;
bool g_sunsetScheduleCacheValid = false;
MaybeBool g_cachedNightLightActive;
bool g_nightLightActiveCacheValid = false;
MaybeBool g_cachedManualSchedule;
bool g_manualScheduleCacheValid = false;

bool ContainsText(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

bool ReadCloudDataSetting(const wchar_t* typeName, std::string* output) {
    std::wstring reader = GetCloudSettingsReaderPath();
    if (reader.empty()) return false;

    std::wstring command = QuotePath(reader) + L" get -type:" + typeName;
    output->clear();
    return RunProcessCapture(command, 1500, output);
}

MaybeBool ReadNightLightSunsetScheduleViaCloudReader() {
    std::string output;
    if (!ReadCloudDataSetting(L"windows.data.bluelightreduction.settings", &output)) {
        return MaybeBool();
    }

    bool scheduleTrue = ContainsText(output, "\"automaticOnSchedule\":true");
    bool scheduleFalse = ContainsText(output, "\"automaticOnSchedule\":false");
    bool sunsetTrue = ContainsText(output, "\"automaticOnSunset\":true");
    bool sunsetFalse = ContainsText(output, "\"automaticOnSunset\":false");

    if (scheduleTrue && sunsetTrue) return MaybeBool(true, true);
    if (scheduleFalse || sunsetFalse) return MaybeBool(true, false);
    return MaybeBool();
}

MaybeBool ReadNightLightActiveViaCloudReader() {
    std::string output;
    if (!ReadCloudDataSetting(L"windows.data.bluelightreduction.bluelightreductionstate", &output)) {
        return MaybeBool();
    }

    if (ContainsText(output, "\"state\":0")) return MaybeBool(true, true);
    if (ContainsText(output, "\"state\":1")) return MaybeBool(true, false);
    return MaybeBool();
}

bool ReadFirstBinary(const wchar_t* const* keys, size_t count, std::vector<BYTE>* data) {
    for (size_t i = 0; i < count; ++i) {
        if (ReadBinaryValue(HKEY_CURRENT_USER, keys[i], L"Data", data)) return true;
    }
    return false;
}

bool ContainsSequence(const std::vector<BYTE>& data, const BYTE* seq, size_t seqSize, size_t start, size_t end) {
    if (seqSize == 0 || data.size() < seqSize || start >= data.size()) return false;
    end = std::min(end, data.size());
    if (end < seqSize) return false;
    for (size_t i = start; i + seqSize <= end; ++i) {
        bool matched = true;
        for (size_t j = 0; j < seqSize; ++j) {
            if (data[i + j] != seq[j]) {
                matched = false;
                break;
            }
        }
        if (matched) return true;
    }
    return false;
}

MaybeBool ReadNightLightActive() {
    std::vector<BYTE> data;
    if (!ReadFirstBinary(kNightLightStateKeys, sizeof(kNightLightStateKeys) / sizeof(kNightLightStateKeys[0]), &data)) {
        return MaybeBool();
    }

    const BYTE marker[] = {0x43, 0x42, 0x01, 0x00};
    const BYTE activeMarker[] = {0x10, 0x00};
    bool sawMarker = false;

    for (size_t markerPos = 0; markerPos + sizeof(marker) <= data.size(); ++markerPos) {
        bool markerMatched = true;
        for (size_t i = 0; i < sizeof(marker); ++i) {
            if (data[markerPos + i] != marker[i]) {
                markerMatched = false;
                break;
            }
        }
        if (!markerMatched) continue;

        sawMarker = true;
        size_t start = markerPos + sizeof(marker);
        size_t end = std::min(start + 10, data.size());
        if (ContainsSequence(data, activeMarker, sizeof(activeMarker), start, end)) {
            return MaybeBool(true, true);
        }
    }

    // Known Windows 10/11 encodings remove the 10 00 field when Night Light is off.
    if (sawMarker) {
        return MaybeBool(true, false);
    }

    return MaybeBool();
}

MaybeBool NightLightLooksLikeManualSchedule() {
    std::vector<BYTE> data;
    if (!ReadFirstBinary(kNightLightSettingsKeys, sizeof(kNightLightSettingsKeys) / sizeof(kNightLightSettingsKeys[0]), &data)) {
        return MaybeBool();
    }

    const BYTE manualOnField[] = {0xCA, 0x14, 0x0E};
    const BYTE manualOffField[] = {0xCA, 0x1E, 0x0E};
    bool hasManualTimes =
        ContainsSequence(data, manualOnField, sizeof(manualOnField), 0, data.size()) &&
        ContainsSequence(data, manualOffField, sizeof(manualOffField), 0, data.size());

    return MaybeBool(true, hasManualTimes);
}

MaybeBool GetNightLightSunsetScheduleCached() {
    if (!g_sunsetScheduleCacheValid) {
        g_cachedSunsetSchedule = ReadNightLightSunsetScheduleViaCloudReader();
        g_sunsetScheduleCacheValid = true;
    }
    return g_cachedSunsetSchedule;
}

MaybeBool GetNightLightActiveCached() {
    if (!g_nightLightActiveCacheValid) {
        g_cachedNightLightActive = ReadNightLightActive();
        if (!g_cachedNightLightActive.known) {
            g_cachedNightLightActive = ReadNightLightActiveViaCloudReader();
        }
        g_nightLightActiveCacheValid = true;
    }
    return g_cachedNightLightActive;
}

MaybeBool NightLightLooksLikeManualScheduleCached() {
    if (!g_manualScheduleCacheValid) {
        g_cachedManualSchedule = NightLightLooksLikeManualSchedule();
        g_manualScheduleCacheValid = true;
    }
    return g_cachedManualSchedule;
}

}  // namespace

void InvalidateScheduleCache() {
    g_sunsetScheduleCacheValid = false;
    g_nightLightActiveCacheValid = false;
    g_manualScheduleCacheValid = false;
}

bool CanFollowWindowsNightLight() {
    MaybeBool schedule = GetNightLightSunsetScheduleCached();
    return schedule.known && schedule.value;
}

bool IsFixedNightNow(const Schedule& schedule) {
    SYSTEMTIME local = {};
    GetLocalTime(&local);
    int now = local.wHour * 60 + local.wMinute;
    int nightStart = schedule.nightStartHour * 60 + schedule.nightStartMinute;
    int dayStart = schedule.dayStartHour * 60 + schedule.dayStartMinute;

    if (nightStart == dayStart) return false;
    if (nightStart < dayStart) {
        return now >= nightStart && now < dayStart;
    }
    return now >= nightStart || now < dayStart;
}

Decision Decide(const Schedule& schedule) {
    if (schedule.followWindowsNightLight) {
        MaybeBool sunsetSchedule = GetNightLightSunsetScheduleCached();
        if (sunsetSchedule.known && sunsetSchedule.value) {
            MaybeBool nightLight = GetNightLightActiveCached();
            if (nightLight.known) {
                Decision decision = {nightLight.value, DecisionSourceWindowsNightLight};
                return decision;
            }
        } else if (!sunsetSchedule.known) {
            MaybeBool manualSchedule = NightLightLooksLikeManualScheduleCached();
            if (!manualSchedule.known || !manualSchedule.value) {
                MaybeBool nightLight = GetNightLightActiveCached();
                if (nightLight.known) {
                    Decision decision = {nightLight.value, DecisionSourceWindowsNightLight};
                    return decision;
                }
            }
        }
    }

    Decision decision = {IsFixedNightNow(schedule), DecisionSourceFixedSchedule};
    return decision;
}

std::wstring FormatTwoDigit(int value) {
    std::wstringstream ss;
    if (value < 10) ss << L"0";
    ss << value;
    return ss.str();
}

std::wstring TimeText(int hour, int minute) {
    std::wstringstream ss;
    ss << FormatTwoDigit(hour) << L":" << FormatTwoDigit(minute);
    return ss.str();
}

void AddMinutesToTime(int* hour, int* minute, int delta) {
    int value = (*hour * 60 + *minute + delta) % (24 * 60);
    if (value < 0) value += 24 * 60;
    *hour = value / 60;
    *minute = value % 60;
}

}  // namespace night_mode
