#pragma once

#include <windows.h>

namespace startup_integration {

bool IsStoreStartupEnabled();
bool SetStoreStartupEnabled(bool enabled);
bool TryReadStoreAppLicenseActive(bool* active);

bool IsPortableStartupEnabled();
bool SetPortableStartupEnabled(bool enabled);
void MigratePortableStartupIfNeeded(bool shouldStartWithWindows);

}
