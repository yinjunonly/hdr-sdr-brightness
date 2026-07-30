#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <cstdio>
#include <string>

#include "../src/store_registry_event_bridge.h"

namespace {

bool Fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}

}  // namespace

int main() {
    const wchar_t* relativePath =
        L"Software\\HdrSdrBrightness\\Tests\\StoreRegistryEventBridge";

    HKEY key = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, relativePath, 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) {
        return Fail("could not create the temporary registry key") ? 0 : 1;
    }

    store_registry_event_bridge::Subscription subscription;
    if (!subscription.Start(relativePath)) {
        RegCloseKey(key);
        RegDeleteTreeW(HKEY_CURRENT_USER,
                       L"Software\\HdrSdrBrightness\\Tests");
        return Fail("could not start the WMI registry event subscription") ? 0 : 1;
    }

    const DWORD marker = GetTickCount();
    if (RegSetValueExW(key, L"Marker", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&marker),
                       sizeof(marker)) != ERROR_SUCCESS) {
        subscription.Stop();
        RegCloseKey(key);
        RegDeleteTreeW(HKEY_CURRENT_USER,
                       L"Software\\HdrSdrBrightness\\Tests");
        return Fail("could not update the temporary registry key") ? 0 : 1;
    }

    const bool changed = subscription.WaitForChange(5000);
    subscription.Stop();
    RegCloseKey(key);
    RegDeleteTreeW(HKEY_CURRENT_USER,
                   L"Software\\HdrSdrBrightness\\Tests");

    if (!changed) {
        return Fail("WMI did not report the host registry change") ? 0 : 1;
    }

    std::puts("PASS: Store WMI bridge observes host registry changes.");
    return 0;
}
