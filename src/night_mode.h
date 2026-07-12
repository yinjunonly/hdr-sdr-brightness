#pragma once

#include <string>

namespace night_mode {

struct Schedule {
    bool followWindowsNightLight;
    int nightStartHour;
    int nightStartMinute;
    int dayStartHour;
    int dayStartMinute;
};

enum DecisionSource {
    DecisionSourceFixedSchedule = 0,
    DecisionSourceWindowsNightLight = 1
};

struct Decision {
    bool night;
    DecisionSource source;
};

void InvalidateScheduleCache();
void InvalidateActiveStateCache();
bool CanFollowWindowsNightLight();
bool IsFixedNightNow(const Schedule& schedule);
Decision Decide(const Schedule& schedule);
std::wstring FormatTwoDigit(int value);
std::wstring TimeText(int hour, int minute);
void AddMinutesToTime(int* hour, int* minute, int delta);

}  // namespace night_mode
