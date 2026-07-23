#define WIN32_LEAN_AND_MEAN

#include "launch_mode.h"

#include <shellapi.h>

namespace launch_mode {
namespace {

bool IsBackgroundLaunchArgument(const wchar_t* arg) {
    if (!arg) return false;
    return lstrcmpiW(arg, L"--background") == 0 ||
           lstrcmpiW(arg, L"/background") == 0 ||
           lstrcmpiW(arg, L"--tray") == 0 ||
           lstrcmpiW(arg, L"/tray") == 0;
}

bool IsStoreFastStartupArgument(const wchar_t* arg) {
    return arg && lstrcmpiW(arg, L"--store-fast-startup") == 0;
}

HWND FindExistingMainWindow() {
    HWND hwnd = FindWindowW(L"HdrSdrBrightnessMainWindow", NULL);
    if (!hwnd) {
        hwnd = FindWindowW(L"HdrSdrSyncMainWindow", NULL);
    }
    if (!hwnd) {
        hwnd = FindWindowW(L"OledHdrSdrSyncMainWindow", NULL);
    }
    return hwnd;
}

}  // namespace

bool HasBackgroundLaunchArgument() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return false;

    bool background = false;
    for (int i = 1; i < argc; ++i) {
        if (IsBackgroundLaunchArgument(argv[i])) {
            background = true;
            break;
        }
    }

    LocalFree(argv);
    return background;
}

bool IsStoreFastStartupLaunch() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return false;

    bool fastStartup = false;
    for (int i = 1; i < argc; ++i) {
        if (IsStoreFastStartupArgument(argv[i])) {
            fastStartup = true;
            break;
        }
    }

    LocalFree(argv);
    return fastStartup;
}

bool ShouldOpenSettingsOnLaunch() {
    return !HasBackgroundLaunchArgument();
}

bool ShowSettingsInExistingInstance(UINT settingsCommandId) {
    HWND hwnd = FindExistingMainWindow();
    if (!hwnd) return false;

    PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(settingsCommandId, 0), 0);
    return true;
}

}  // namespace launch_mode
