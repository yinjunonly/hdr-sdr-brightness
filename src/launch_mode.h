#pragma once

#include <windows.h>

namespace launch_mode {

bool HasBackgroundLaunchArgument();
bool ShouldOpenSettingsOnLaunch();
bool ShowSettingsInExistingInstance(UINT settingsCommandId);

}  // namespace launch_mode
