#pragma once

#include <windows.h>

namespace startup_integration {

bool IsStoreStartupEnabled();
bool SetStoreStartupEnabled(bool enabled);
void RepairStoreFastStartupIfNeeded();
bool ShouldRunStoreFastStartup();
bool TryReadStoreAppLicenseActive(bool* active);

bool IsPortableStartupEnabled();
bool SetPortableStartupEnabled(bool enabled);
void RepairPortableScheduledTaskStartupIfNeeded(bool shouldStartWithWindows);

}
