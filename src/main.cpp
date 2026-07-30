#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#define _WIN32_IE 0x0600

#include <windows.h>
#include <windowsx.h>
#include <propidl.h>
#include <gdiplus.h>
#include <objbase.h>
#include <shellapi.h>

#ifndef CLEARTYPE_NATURAL_QUALITY
#define CLEARTYPE_NATURAL_QUALITY 6
#endif

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "localization.h"
#include "version.h"
#include "app_hotkeys.h"
#include "brightness_initialization.h"
#include "capture_pipe.h"
#include "capture_paths.h"
#include "capture_request_queue.h"
#include "display_brightness.h"
#include "editor_window_control.h"
#include "fullscreen_capture_adapter.h"
#include "hdr_preview.h"
#include "launch_mode.h"
#include "night_mode.h"
#include "process_util.h"
#include "registry_util.h"
#include "registry_watcher.h"
#include "startup_integration.h"
#include "supporter_code.h"
#include "tray_icon.h"
#include "ui_backbuffer.h"
#include "ui_dpi.h"
#include "ui_gdiplus.h"
#include "ui_theme.h"
#include "ui_window.h"

namespace {

const wchar_t kDisplayName[] = L"HDR SDR Brightness";
const wchar_t kConfigKey[] = L"Software\\OledHdrSdrSync";
const int IDI_APPICON = 101;

const UINT kTrayMessage = WM_APP + 1;
const UINT kApplyMessage = WM_APP + 2;
const UINT kRegistryChangedMessage = WM_APP + 3;
const UINT kStoreLicenseExpiredMessage = WM_APP + 4;
const UINT_PTR kRecheckTimer = 1;
const UINT_PTR kTransitionTimer = 2;
const UINT_PTR kSettingsAnimationTimer = 3;
const UINT_PTR kSupportCaretTimer = 4;
const UINT_PTR kCaptureWarmupTimer = 5;
const UINT kRecheckMs = 15 * 1000;
const UINT kTransitionMs = 45;
const UINT kSettingsAnimationMs = 33;
const UINT kCaptureWarmupMs = 250;
const UINT32 kTransitionStepLevel = 50;
const int kSettingsClientWidth = 640;
const int kSettingsClientHeight = 620;
const int kSettingsMinVisibleClientHeight = 560;
const int kSettingsTitleBarHeight = 44;
const int kSettingsFooterAreaHeight = 72;
const int kSettingsCardPadding = 36;
const int kSettingsCardTopPadding = 16;
const int kSettingsRightControlWidth = 220;
const int kAnimationSlotCount = 800;
const int kPillControlRadius = 16;
const int kStoreSupportButtonW = 168;
const int kStoreSupportButtonH = 54;
const int kStoreSupportButtonY = 22;
const size_t kSupporterCodeMaxLength = 20;

const UINT kMenuApply = 1001;
const UINT kMenuSettings = 1002;
const UINT kMenuStartup = 1003;
const UINT kMenuDisplaySettings = 1004;
const UINT kMenuNightLightSettings = 1005;
const UINT kMenuExit = 1006;
const UINT kMenuSupport = 1007;
const UINT kMenuHdrCalibration = 1008;
const UINT kMenuHdrScreenshot = 1009;

const int kHotkeyIdScreenshot = 1;
const int kHotkeyIdFullscreen = 2;
const UINT kFullscreenDoneMessage = WM_APP + 5;
const UINT kRegionCaptureDoneMessage = WM_APP + 6;

const int kIdDayBrightness = 2001;
const int kIdNightBrightness = 2002;
const int kIdFollowNightLight = 2003;
const int kIdStartup = 2004;
const int kIdNightStartHour = 2005;
const int kIdNightStartMinute = 2006;
const int kIdDayStartHour = 2007;
const int kIdDayStartMinute = 2008;
const int kIdApply = 2009;
const int kIdOk = 2010;
const int kIdCancel = 2011;
const int kIdLanguage = 2012;
const int kIdAutoRestoreManual = 2013;
const int kIdSupportDonate = 2020;
const int kIdSupportActivate = 2021;
const int kIdSupportCode = 2023;
const int kIdSupportStatus = 2024;

const wchar_t kStoreSupportUrl[] = L"https://apps.microsoft.com/detail/9nksvcpjl35j?cid=github-build";
const wchar_t kGithubUrl[] = L"https://github.com/yinjunonly/hdr-sdr-brightness";
const wchar_t kHdrCalibrationStoreUri[] = L"ms-windows-store://pdp/?ProductId=9N7F2SM5D1LR";
const wchar_t kHdrCalibrationWebUrl[] = L"https://apps.microsoft.com/detail/9n7f2sm5d1lr";

#ifndef NIN_SELECT
#define NIN_SELECT (WM_USER + 0)
#endif
#ifndef NIN_BALLOONUSERCLICK
#define NIN_BALLOONUSERCLICK (WM_USER + 5)
#endif
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

enum HoverControl {
    HoverNone = 0,
    HoverLanguageButton = 100,
    HoverLanguageOptionBase = 110,
    HoverSwitchBase = 200,
    HoverDaySlider = 301,
    HoverNightSlider = 302,
    HoverAutoRestore = 401,
    HoverStartup = 402,
    HoverNightMinus = 501,
    HoverNightPlus = 502,
    HoverDayMinus = 503,
    HoverDayPlus = 504,
    HoverOk = 601,
    HoverApply = 602,
    HoverCancel = 603,
    HoverSupport = 604,
    HoverSupporterBadge = 605,
    HoverHdrCalibration = 606,
    HoverHdrCalibrationDismiss = 607,
    HoverTitleHelp = 611,
    HoverTitleGithub = 612,
    HoverTitleMinimize = 613,
    HoverTitleClose = 614,
    HoverDialogOk = 701,
    HoverDialogClose = 702,
    HoverDialogLink = 703,
    HoverHotkeySettings = 704,
    HoverNotifyOk = 711,
    HoverNotifySettings = 712,
    HoverSupportDonate = 721,
    HoverSupportActivate = 722,
    HoverSupportCode = 724,
    HoverScreenshotHotkey = 750,
    HoverFullscreenHotkey = 751
};

enum NotificationAction {
    NotificationActionDefault = 0,
    NotificationActionSupportReminder = 1,
    NotificationActionHdrCalibration = 2,
    NotificationActionHdrScreenshot = 3,
    NotificationActionFullscreenScreenshot = 4
};

struct Config {
    int dayBrightness;
    int nightBrightness;
    bool followNightLight;
    bool autoRestoreManualChanges;
    bool startWithWindows;
    std::wstring supporterCode;
    int language;
    int nightStartHour;
    int nightStartMinute;
    int dayStartHour;
    int dayStartMinute;
    UINT screenshotHotkeyMod;
    UINT screenshotHotkeyVk;
    UINT fullscreenHotkeyMod;
    UINT fullscreenHotkeyVk;

    Config()
        : dayBrightness(40),
          nightBrightness(25),
          followNightLight(true),
          autoRestoreManualChanges(true),
          startWithWindows(false),
          supporterCode(),
          language(LangAuto),
          nightStartHour(18),
          nightStartMinute(0),
          dayStartHour(8),
          dayStartMinute(0),
          screenshotHotkeyMod(MOD_ALT),
          screenshotHotkeyVk('S'),
          fullscreenHotkeyMod(MOD_ALT | MOD_SHIFT),
          fullscreenHotkeyVk('S') {}
};

struct NightDecision {
    bool night;
    std::wstring source;
};

HINSTANCE g_instance = NULL;
HWND g_mainWindow = NULL;
HWND g_settingsWindow = NULL;
tray_icon::TrayIcon g_trayIcon;
UINT g_taskbarCreated = 0;
Config g_config;
Config g_settingsDraft;
bool g_brightnessConfigReady = false;
bool g_settingsDraftActive = false;
bool g_hdrCalibrationCalloutDismissed = false;
std::wstring g_status = L"Starting";
std::wstring g_lastNotificationTitle;
std::wstring g_lastNotificationBody;
NotificationAction g_lastNotificationAction = NotificationActionDefault;
std::wstring g_lastFullscreenCapturePath;
int g_lastAppliedBrightness = -1;
bool g_lastDecisionNight = false;
int g_lastHdrTargetCount = 0;
int g_lastHdrSuccessCount = 0;
UINT32 g_lastKnownTargetLevel = 0;
UINT32 g_transitionTargetLevel = 0;
int g_transitionTargetBrightness = -1;
bool g_transitionActive = false;
bool g_transitionManualCorrection = false;
bool g_transitionNight = false;
std::wstring g_transitionSource;
DWORD g_lastManualNotificationTick = 0;
int g_draggingBrightnessId = 0;
int g_recordingHotkey = 0;  // 0=无，HoverScreenshotHotkey 或 HoverFullscreenHotkey 录制中
bool g_settingsPreviewActive = false;
int g_settingsPreviewBrightness = -1;
bool g_settingsPreviewNight = false;
bool g_languageDropdownOpen = false;
bool g_settingsInfoDialogOpen = false;
bool g_hotkeyDialogOpen = false;
HWND g_notificationWindow = NULL;
HWND g_supportWindow = NULL;
HWND g_supportOwnerWindow = NULL;
DWORD g_ignoreSettingsMouseUntil = 0;
std::wstring g_supportStatus;
std::wstring g_supportCodeInput;
bool g_supportCodeFocused = false;
bool g_supportCaretVisible = false;
int g_supportPressedControl = 0;
int g_settingsScrollY = 0;
int g_hoverControl = 0;
int g_pressedControl = 0;
bool g_trackingSettingsMouse = false;
POINT g_settingsMousePoint = {-10000, -10000};
bool g_settingsMouseKnown = false;
int g_settingsWindowAnim = 0;
int g_settingsDialogAnim = 0;
int g_settingsDropdownAnim = 0;
int g_supportButtonAnim = 0;
int g_controlAnim[kAnimationSlotCount] = {};
registry_watcher::Watcher g_registryWatcher;
HFONT g_uiFont = NULL;
HFONT g_smallFont = NULL;
HFONT g_titleFont = NULL;
HFONT g_sectionFont = NULL;
HFONT g_heroFont = NULL;
HBRUSH g_windowBrush = NULL;
HBRUSH g_panelBrush = NULL;
HBRUSH g_editBrush = NULL;
HWND g_hdrPreviewWindow = NULL;
HdrPreviewRenderer g_hdrPreview;

ui_theme::Theme g_theme = {};
using ui_theme::Rgb;

int ClampInt(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

int ResolveSystemUiLanguage() {
    LANGID langId = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(langId)) {
    case LANG_CHINESE:
        switch (SUBLANGID(langId)) {
        case 0x01:  // Traditional Chinese.
        case 0x03:  // Hong Kong SAR.
        case 0x05:  // Macao SAR.
            return LangChineseTraditional;
        default:
            break;
        }
        return LangChinese;
    case LANG_KOREAN:
        return LangKorean;
    case LANG_JAPANESE:
        return LangJapanese;
    case LANG_RUSSIAN:
        return LangRussian;
    case LANG_GERMAN:
        return LangGerman;
    default:
        return LangEnglish;
    }
}

int CurrentUiLanguage() {
    int language = g_settingsDraftActive ? g_settingsDraft.language : g_config.language;
    language = NormalizeLanguageChoice(language);
    return language == LangAuto ? ResolveSystemUiLanguage() : language;
}

const wchar_t* T(TextId id) {
    return LocalizedText(CurrentUiLanguage(), id);
}

std::wstring F(TextId id, std::initializer_list<std::wstring> args) {
    return FormatLocalizedText(CurrentUiLanguage(), id, args);
}

std::wstring IntText(int value) {
    std::wstringstream ss;
    ss << value;
    return ss.str();
}

void UpdateSettingsWindowTitle(HWND hwnd) {
    if (hwnd) SetWindowTextW(hwnd, T(TxtSettingsTitle));
}

std::wstring PercentLabel(int value);
int TextWidthLogical(HDC dc, const wchar_t* text, HFONT font);
void CleanupFontResources();
void ShowTrayNotification(const std::wstring& title, const std::wstring& body,
                          NotificationAction action);
void ShowNotificationDialogWindow();
void ShowSettingsWindow(HWND owner);
void ShowSupportWindow(HWND owner);

void OpenGithubRepository(HWND owner) {
    ShellExecuteW(owner, L"open", kGithubUrl, NULL, NULL, SW_SHOWNORMAL);
}

void OpenHdrCalibration(HWND owner) {
    HINSTANCE result = ShellExecuteW(owner, L"open", kHdrCalibrationStoreUri, NULL, NULL, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        ShellExecuteW(owner, L"open", kHdrCalibrationWebUrl, NULL, NULL, SW_SHOWNORMAL);
    }
}

std::wstring AppVersionLabel() {
    return std::wstring(L"v") + APP_VERSION_W;
}

bool IsStoreBuild() {
#ifdef HSB_STORE_BUILD
    return true;
#else
    return false;
#endif
}

bool IsSupportFeatureAvailable() {
    return !IsStoreBuild();
}

bool IsStartupFeatureAvailable() {
    return true;
}

bool UseStoreStartupIntegration() {
    return IsStoreBuild();
}

bool g_nativeCaptureServerStartAttempted = false;
DWORD g_nativeCaptureServerStartTick = 0;
bool g_editorWarmupAttempted = false;
capture_request::Queue g_regionCaptureRequests;

std::wstring GetFullscreenNotificationCapturePath() {
    return capture_paths::FullscreenNotificationBmpPath();
}

std::wstring GetRegionEditCapturePath() {
    return capture_paths::RegionEditBmpPath();
}

std::wstring ReplaceExtension(const std::wstring& path, const std::wstring& extension) {
    return capture_paths::ReplaceExtension(path, extension);
}

std::wstring CurrentSdrWhiteLevelCommandField() {
    UINT32 level = g_lastKnownTargetLevel;
    if (level == 0 && g_lastAppliedBrightness >= 0) {
        level = BrightnessPercentToSdrLevel(g_lastAppliedBrightness);
    }
    return level > 0 ? IntText(static_cast<int>(level)) : L"";
}

std::wstring SdrWhiteArgumentFromLevel(UINT32 level) {
    if (level == 0) return L"";
    std::wstringstream ss;
    ss << (level / 1000u) << L".";
    UINT32 fraction = level % 1000u;
    if (fraction < 100u) ss << L"0";
    if (fraction < 10u) ss << L"0";
    ss << fraction;
    return ss.str();
}

std::wstring CurrentSdrWhiteCommandLineArgument() {
    UINT32 level = g_lastKnownTargetLevel;
    if (level == 0 && g_lastAppliedBrightness >= 0) {
        level = BrightnessPercentToSdrLevel(g_lastAppliedBrightness);
    }
    std::wstring value = SdrWhiteArgumentFromLevel(level);
    return value.empty() ? L"" : L" --sdr-white " + value;
}

void StartNativeCaptureHelperServer() {
    if (g_nativeCaptureServerStartAttempted) return;

    std::wstring command = fullscreen_capture::BuildNativeServerCommand(CurrentUiLanguage());
    if (command.empty()) return;

    std::wstring helperPath = fullscreen_capture::GetNativeHelperPath();
    g_nativeCaptureServerStartAttempted = true;
    g_nativeCaptureServerStartTick = GetTickCount();

    if (!LaunchDetachedHidden(command, DirectoryFromPath(helperPath))) {
        g_nativeCaptureServerStartAttempted = false;
        g_nativeCaptureServerStartTick = 0;
    }
}

void StartNativeEditorWarmup() {
    if (g_editorWarmupAttempted) return;

    std::wstring command = fullscreen_capture::BuildEditorWarmupCommand();
    if (command.empty()) return;

    std::wstring helperPath = fullscreen_capture::GetEditorHelperPath();
    g_editorWarmupAttempted = true;
    if (!LaunchDetachedHidden(command, DirectoryFromPath(helperPath))) {
        g_editorWarmupAttempted = false;
    }
}

struct RegionCaptureArgs {
    std::uint64_t generation;
    HWND mainWnd;
    std::wstring nativePipeName;
    std::wstring nativePipeCommand;
    std::wstring nativeCommand;
    std::wstring editCommand;
};

struct RegionCaptureResult {
    std::uint64_t generation;
    bool captured;
    bool helperMissing;
    std::wstring editCommand;
};

void PostRegionCaptureResult(HWND mainWnd,
                             std::uint64_t generation,
                             bool captured,
                             bool helperMissing,
                             const std::wstring& editCommand) {
    auto* result = new RegionCaptureResult{
        generation, captured, helperMissing, editCommand
    };
    if (!PostMessageW(mainWnd, kRegionCaptureDoneMessage, 0,
                      reinterpret_cast<LPARAM>(result))) {
        delete result;
    }
}

static DWORD WINAPI RegionCaptureThread(LPVOID param) {
    auto* args = static_cast<RegionCaptureArgs*>(param);
    bool captured = false;
    DWORD exitCode = 1;
    if (!args->nativePipeName.empty() && !args->nativePipeCommand.empty()) {
        bool sent = capture_pipe::SendCommandForExitCodeToPipe(
            args->nativePipeName, args->nativePipeCommand, 30000, &exitCode);
        captured = sent && exitCode == 0;
        if (!sent) g_nativeCaptureServerStartAttempted = false;
    }
    if (!captured) {
        bool completed = RunHiddenCommandExitCode(args->nativeCommand, 600000, &exitCode);
        captured = completed && exitCode == 0;
    }
    PostRegionCaptureResult(args->mainWnd, args->generation, captured, false,
                            args->editCommand);
    delete args;
    return 0;
}

void StartRegionCapture(HWND owner, std::uint64_t generation) {
    std::wstring regionPath = GetRegionEditCapturePath();
    std::wstring nativeCommand = fullscreen_capture::BuildCaptureFileCommand(
        regionPath,
        CurrentUiLanguage(),
        CurrentSdrWhiteCommandLineArgument());
    std::wstring editCommand = fullscreen_capture::BuildEditorSelectCommand(
        regionPath,
        ReplaceExtension(regionPath, L".png"),
        CurrentUiLanguage());
    if (nativeCommand.empty() || editCommand.empty()) {
        PostRegionCaptureResult(owner, generation, false, true, L"");
        return;
    }

    StartNativeCaptureHelperServer();
    StartNativeEditorWarmup();
    auto* args = new RegionCaptureArgs{
        generation,
        owner,
        fullscreen_capture::GetNativePipeName(),
        fullscreen_capture::BuildCaptureFilePipeCommand(
            CurrentUiLanguage(), regionPath, CurrentSdrWhiteLevelCommandField()),
        nativeCommand,
        editCommand
    };
    HANDLE thread = CreateThread(NULL, 0, RegionCaptureThread, args, 0, NULL);
    if (thread) {
        CloseHandle(thread);
        return;
    }
    delete args;
    PostRegionCaptureResult(owner, generation, false, false, editCommand);
}

void LaunchHdrScreenshotHelper(HWND owner) {
    editor_window_control::CloseAll();
    capture_request::RequestDecision request = g_regionCaptureRequests.Request();
    if (request.startCapture) {
        StartRegionCapture(owner, request.generation);
    }
}

void LaunchHdrFullscreenEditor(HWND owner) {
    (void)owner;
    g_regionCaptureRequests.Cancel();
    if (g_lastFullscreenCapturePath.empty() || !FileExists(g_lastFullscreenCapturePath)) {
        ShowTrayNotification(T(TxtMenuHdrScreenshot), T(TxtCaptureLaunchFailed), NotificationActionDefault);
        return;
    }

    std::wstring command = fullscreen_capture::BuildEditorEditCommand(
        g_lastFullscreenCapturePath,
        ReplaceExtension(g_lastFullscreenCapturePath, L".png"),
        CurrentUiLanguage(),
        true);
    if (command.empty()) {
        ShowTrayNotification(T(TxtMenuHdrScreenshot), T(TxtCaptureHelperMissing), NotificationActionDefault);
        return;
    }

    editor_window_control::CloseAll();
    if (!LaunchDetached(command, DirectoryFromPath(fullscreen_capture::GetEditorHelperPath()))) {
        ShowTrayNotification(T(TxtMenuHdrScreenshot), T(TxtCaptureLaunchFailed), NotificationActionDefault);
    }
}

void UnregisterAppHotkeys() {
    app_hotkeys::Unregister(g_mainWindow, kHotkeyIdScreenshot, kHotkeyIdFullscreen);
}

void RegisterAppHotkeys() {
    app_hotkeys::Register(g_mainWindow, kHotkeyIdScreenshot,
                          {g_config.screenshotHotkeyMod, g_config.screenshotHotkeyVk},
                          kHotkeyIdFullscreen,
                          {g_config.fullscreenHotkeyMod, g_config.fullscreenHotkeyVk});
}

bool IsStoreLicenseExplicitlyInactive() {
    if (!IsStoreBuild()) return false;

    bool active = true;
    if (!startup_integration::TryReadStoreAppLicenseActive(&active)) {
        return false;
    }

    return !active;
}

static DWORD WINAPI StoreLicenseCheckThread(LPVOID param) {
    HWND hwnd = reinterpret_cast<HWND>(param);
    if (hwnd && IsStoreLicenseExplicitlyInactive()) {
        PostMessageW(hwnd, kStoreLicenseExpiredMessage, 0, 0);
    }
    return 0;
}

void StartStoreLicenseCheckThread(HWND hwnd) {
    if (!UseStoreStartupIntegration() || !hwnd) return;
    HANDLE thread = CreateThread(NULL, 0, StoreLicenseCheckThread, hwnd, 0, NULL);
    if (thread) CloseHandle(thread);
}

void DisableStoreStartupAfterLicenseExpired() {
    if (!UseStoreStartupIntegration()) return;
    startup_integration::SetStoreStartupEnabled(false);
    g_config.startWithWindows = false;
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"StartWithWindows", 0);
}

// 全屏截图后台线程参数包
struct FullscreenCaptureArgs {
    std::wstring nativePipeName;
    std::wstring nativePipeCommand;
    std::wstring nativeCommand;
    HWND mainWnd;
};

static DWORD WINAPI FullscreenCaptureThread(LPVOID param) {
    auto* args = static_cast<FullscreenCaptureArgs*>(param);
    bool success = false;
    if (!args->nativePipeName.empty() && !args->nativePipeCommand.empty()) {
        DWORD exitCode = 1;
        bool sent = capture_pipe::SendCommandForExitCodeToPipe(args->nativePipeName,
                                                               args->nativePipeCommand,
                                                               30000,
                                                               &exitCode);
        success = sent && exitCode == 0;
        if (!sent) {
            g_nativeCaptureServerStartAttempted = false;
        }
    }
    if (!success && !args->nativeCommand.empty()) {
        success = RunHiddenCommand(args->nativeCommand, 30000);
    }
    PostMessageW(args->mainWnd, kFullscreenDoneMessage, success ? 1 : 0, 0);
    delete args;
    return 0;
}

// 全屏截图：后台线程执行，完成后通过消息通知主窗口
void LaunchHdrFullscreenCapture(HWND owner) {
    (void)owner;
    g_regionCaptureRequests.Cancel();
    editor_window_control::CloseAll();
    g_lastFullscreenCapturePath = GetFullscreenNotificationCapturePath();
    std::wstring nativeCommand = fullscreen_capture::BuildFullscreenCommand(
        g_lastFullscreenCapturePath,
        CurrentUiLanguage(),
        CurrentSdrWhiteCommandLineArgument());
    if (nativeCommand.empty()) {
        ShowTrayNotification(T(TxtMenuHdrScreenshot), T(TxtCaptureHelperMissing), NotificationActionDefault);
        return;
    }

    StartNativeCaptureHelperServer();
    auto* args = new FullscreenCaptureArgs{
        fullscreen_capture::GetNativePipeName(),
        fullscreen_capture::BuildFullscreenPipeCommand(
            CurrentUiLanguage(),
            g_lastFullscreenCapturePath,
            CurrentSdrWhiteLevelCommandField()),
        nativeCommand,
        g_mainWindow
    };
    HANDLE hThread = CreateThread(NULL, 0, FullscreenCaptureThread, args, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        delete args;
        PostMessageW(g_mainWindow, kFullscreenDoneMessage, 0, 0);
    }
}

bool HasSupporterBadge() {
    return false;
}

int SupportButtonWidth(HDC dc) {
    UNREFERENCED_PARAMETER(dc);
    return kStoreSupportButtonW;
}

int SupportButtonLeft(HDC dc) {
    return 28 + 584 - kSettingsCardPadding - SupportButtonWidth(dc);
}

TextId StoreBubbleTextId() {
    switch ((g_supportButtonAnim / 40) % 3) {
    case 1:
        return TxtStoreBubbleUpdates;
    case 2:
        return TxtStoreBubbleSupport;
    case 0:
    default:
        return TxtStoreBubbleDownload;
    }
}

int StoreBubbleMode() {
    return (g_supportButtonAnim / 40) % 3;
}

int SupporterBadgeWidth(HDC dc) {
    return ClampInt(TextWidthLogical(dc, T(TxtSupporterBadge), g_smallFont) + 58, 116, 154);
}

int SupporterBadgeLeft(HDC dc) {
    return 28 + 584 - kSettingsCardPadding - SupporterBadgeWidth(dc);
}

bool IsStartupEnabled() {
    if (UseStoreStartupIntegration()) return startup_integration::IsStoreStartupEnabled();

    return startup_integration::IsPortableStartupEnabled();
}

bool TrySetStartupEnabled(bool enabled) {
    if (UseStoreStartupIntegration()) {
        return startup_integration::SetStoreStartupEnabled(enabled);
    }

    return startup_integration::SetPortableStartupEnabled(enabled);
}

static DWORD WINAPI StartupRepairThread(LPVOID) {
    startup_integration::MigratePortableStartupIfNeeded(g_config.startWithWindows);
    return 0;
}

void StartStartupRepairThread() {
    if (UseStoreStartupIntegration()) return;
    HANDLE thread = CreateThread(NULL, 0, StartupRepairThread, NULL, 0, NULL);
    if (thread) CloseHandle(thread);
}

void LoadConfig(bool refreshStartupState = false) {
    DWORD value = 0;
    DWORD dayValue = 0;
    DWORD nightValue = 0;
    DWORD initializationMarker = 0;
    brightness_initialization::StoredState stored = {};
    stored.hasDay =
        ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"DayBrightness", &dayValue);
    stored.dayBrightness = ClampInt(static_cast<int>(dayValue), 0, 100);
    stored.hasNight =
        ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"NightBrightness", &nightValue);
    stored.nightBrightness = ClampInt(static_cast<int>(nightValue), 0, 100);
    stored.markerSet =
        ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"BrightnessDefaultsInitialized",
                       &initializationMarker) &&
        initializationMarker != 0;

    int currentBrightness = 0;
    bool needsCurrentBrightness =
        !stored.markerSet && (!stored.hasDay || !stored.hasNight);
    bool hasCurrentBrightness =
        needsCurrentBrightness && ReadCurrentSdrBrightness(&currentBrightness);
    brightness_initialization::Resolution brightnessResolution =
        brightness_initialization::Resolve(stored, hasCurrentBrightness, currentBrightness);
    g_brightnessConfigReady = brightnessResolution.ready;
    if (brightnessResolution.ready) {
        g_config.dayBrightness = brightnessResolution.dayBrightness;
        g_config.nightBrightness = brightnessResolution.nightBrightness;
    }
    if (brightnessResolution.writeDay) {
        WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"DayBrightness",
                        brightnessResolution.dayBrightness);
    }
    if (brightnessResolution.writeNight) {
        WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"NightBrightness",
                        brightnessResolution.nightBrightness);
    }
    if (brightnessResolution.writeMarker) {
        WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"BrightnessDefaultsInitialized", 1);
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"FollowNightLight", &value)) {
        g_config.followNightLight = value != 0;
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"AutoRestoreManualChanges", &value)) {
        g_config.autoRestoreManualChanges = value != 0;
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"StartWithWindows", &value)) {
        g_config.startWithWindows = value != 0;
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"HdrCalibrationCalloutDismissed", &value)) {
        g_hdrCalibrationCalloutDismissed = value != 0;
    }
    std::wstring stringValue;
    if (IsSupportFeatureAvailable() &&
        ReadStringValue(HKEY_CURRENT_USER, kConfigKey, L"SupporterCode", &stringValue)) {
        g_config.supporterCode = supporter_code::Normalize(stringValue);
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"Language", &value)) {
        g_config.language = NormalizeLanguageChoice(static_cast<int>(value));
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"NightStartHour", &value)) {
        g_config.nightStartHour = ClampInt(static_cast<int>(value), 0, 23);
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"NightStartMinute", &value)) {
        g_config.nightStartMinute = ClampInt(static_cast<int>(value), 0, 59);
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"DayStartHour", &value)) {
        g_config.dayStartHour = ClampInt(static_cast<int>(value), 0, 23);
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"DayStartMinute", &value)) {
        g_config.dayStartMinute = ClampInt(static_cast<int>(value), 0, 59);
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"ScreenshotHotkeyMod", &value)) {
        g_config.screenshotHotkeyMod = value & app_hotkeys::ModifierMask();
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"ScreenshotHotkeyVk", &value)) {
        g_config.screenshotHotkeyVk = value;
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"FullscreenHotkeyMod", &value)) {
        g_config.fullscreenHotkeyMod = value & app_hotkeys::ModifierMask();
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"FullscreenHotkeyVk", &value)) {
        g_config.fullscreenHotkeyVk = value;
    }
    if (!IsStartupFeatureAvailable()) {
        g_config.startWithWindows = false;
    } else if (refreshStartupState) {
        g_config.startWithWindows = IsStartupEnabled();
    }
    if (IsLocalizedTextValue(TxtStarting, g_status)) {
        g_status = T(TxtStarting);
    }
}

void SaveConfig() {
    if (g_brightnessConfigReady) {
        WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"DayBrightness", g_config.dayBrightness);
        WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"NightBrightness", g_config.nightBrightness);
        WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"BrightnessDefaultsInitialized", 1);
    }
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"FollowNightLight", g_config.followNightLight ? 1 : 0);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"AutoRestoreManualChanges", g_config.autoRestoreManualChanges ? 1 : 0);
    if (IsSupportFeatureAvailable()) {
        WriteStringValue(HKEY_CURRENT_USER, kConfigKey, L"SupporterCode", g_config.supporterCode);
    }
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"Language", static_cast<DWORD>(g_config.language));
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"NightStartHour", g_config.nightStartHour);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"NightStartMinute", g_config.nightStartMinute);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"DayStartHour", g_config.dayStartHour);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"DayStartMinute", g_config.dayStartMinute);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"ScreenshotHotkeyMod", g_config.screenshotHotkeyMod);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"ScreenshotHotkeyVk", g_config.screenshotHotkeyVk);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"FullscreenHotkeyMod", g_config.fullscreenHotkeyMod);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"FullscreenHotkeyVk", g_config.fullscreenHotkeyVk);
    bool requestedStartup = g_config.startWithWindows;
    bool startupSetOk = TrySetStartupEnabled(requestedStartup);
    if (UseStoreStartupIntegration()) {
        g_config.startWithWindows = IsStartupEnabled();
        if (requestedStartup && !g_config.startWithWindows && !startupSetOk) {
            ShowTrayNotification(T(TxtStartWithWindows),
                                 L"Windows did not enable startup for this packaged app. Enable HDR SDR Brightness Assistant in Windows Settings > Apps > Startup.",
                                 NotificationActionDefault);
            ShellExecuteW(g_mainWindow, L"open", L"ms-settings:startupapps", NULL, NULL, SW_SHOWNORMAL);
        }
    }
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"StartWithWindows", g_config.startWithWindows ? 1 : 0);
}

night_mode::Schedule CurrentNightSchedule() {
    night_mode::Schedule schedule = {
        g_config.followNightLight,
        g_config.nightStartHour,
        g_config.nightStartMinute,
        g_config.dayStartHour,
        g_config.dayStartMinute
    };
    return schedule;
}

void InvalidateNightLightScheduleCache() {
    night_mode::InvalidateScheduleCache();
}

bool CanFollowWindowsNightLight() {
    return night_mode::CanFollowWindowsNightLight();
}

NightDecision DecideNight() {
    night_mode::Decision raw = night_mode::Decide(CurrentNightSchedule());

    NightDecision decision;
    decision.night = raw.night;
    if (raw.source == night_mode::DecisionSourceWindowsNightLight) {
        decision.source = T(TxtSourceNightLight);
    } else {
        decision.source = F(TxtFixedScheduleSource,
                            {T(TxtSourceFixed),
                             night_mode::TimeText(g_config.nightStartHour, g_config.nightStartMinute),
                             night_mode::TimeText(g_config.dayStartHour, g_config.dayStartMinute)});
    }
    return decision;
}

std::wstring BuildStatusText(const NightDecision& decision, int brightness, const ApplyResult& result) {
    std::wstringstream ss;
    ss << F(TxtStatusBrightness, {decision.night ? T(TxtNight) : T(TxtDay), PercentLabel(brightness)});
    ss << L" " << T(TxtVia) << L" " << decision.source << L". ";

    if (result.ok) {
        if (result.usedDwmFallback) {
            ss << T(TxtAppliedDwm);
        } else {
            ss << F(TxtAppliedToDisplays, {IntText(result.successCount), IntText(result.targetCount)});
        }
    } else if (result.targetCount == 0) {
        ss << T(TxtNoHdrDisplay);
    } else {
        ss << F(TxtApplyFailed, {IntText(result.lastError)});
    }

    return ss.str();
}

void UpdateTrayTip() {
    if (!g_mainWindow) return;
    std::wstringstream tip;
    tip << T(TxtDisplayName) << L" - ";
    if (g_transitionActive && g_transitionTargetBrightness >= 0) {
        tip << F(TxtTrayRestoringBrightness, {PercentLabel(g_transitionTargetBrightness)});
    } else if (g_lastAppliedBrightness >= 0) {
        tip << F(TxtTrayBrightness, {g_lastDecisionNight ? T(TxtNight) : T(TxtDay),
                                     PercentLabel(g_lastAppliedBrightness)});
    } else {
        tip << T(TxtStarting);
    }

    tip << L" - ";
    if (g_lastHdrTargetCount > 0) {
        tip << F(TxtHdrDisplays, {IntText(g_lastHdrSuccessCount), IntText(g_lastHdrTargetCount)});
    } else {
        tip << T(TxtNoHdrShort);
    }

    tip << L" - ";
    tip << T(TxtAutoRestoreShort) << L" "
        << (g_config.autoRestoreManualChanges ? T(TxtOn) : T(TxtOff));

    g_trayIcon.UpdateTip(tip.str());
}

void ShowTrayNotification(const std::wstring& title, const std::wstring& body,
                          NotificationAction action = NotificationActionDefault) {
    if (!g_mainWindow) return;
    g_lastNotificationTitle = title;
    g_lastNotificationBody = body;
    g_lastNotificationAction = action;

    g_trayIcon.ShowNotification(title, body);
}

void ShowLastNotificationDialog() {
    if (g_lastNotificationBody.empty()) return;
    if (g_lastNotificationAction == NotificationActionSupportReminder) {
        ShowSupportWindow(g_mainWindow);
        return;
    }
    if (g_lastNotificationAction == NotificationActionHdrCalibration) {
        OpenHdrCalibration(g_mainWindow);
        return;
    }
    if (g_lastNotificationAction == NotificationActionHdrScreenshot) {
        LaunchHdrScreenshotHelper(g_mainWindow);
        return;
    }
    if (g_lastNotificationAction == NotificationActionFullscreenScreenshot) {
        LaunchHdrFullscreenEditor(g_mainWindow);
        return;
    }
    ShowNotificationDialogWindow();
}

void NotifyManualCorrection(int brightness) {
    DWORD now = GetTickCount();
    if (g_lastManualNotificationTick != 0 && now - g_lastManualNotificationTick < 30000) return;
    g_lastManualNotificationTick = now;

    std::wstringstream body;
    body << F(TxtNotifyBody, {PercentLabel(brightness)}) << L" "
         << F(TxtNotifyManualRestoreHint, {T(TxtAutoRestoreManual)});
    ShowTrayNotification(T(TxtNotifyTitle), body.str());
}

DWORD SupportReminderDateValue(const SYSTEMTIME& local) {
    return static_cast<DWORD>(local.wYear) * 10000u +
           static_cast<DWORD>(local.wMonth) * 100u +
           static_cast<DWORD>(local.wDay);
}

void CheckWeeklySupportReminder() {
    if (!IsSupportFeatureAvailable()) return;
    if (HasSupporterBadge()) return;

    SYSTEMTIME local = {};
    GetLocalTime(&local);
    if (local.wDayOfWeek != 6 || local.wHour != 20) return;

    DWORD today = SupportReminderDateValue(local);
    DWORD lastReminderDate = 0;
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"LastSupportReminderDate", &lastReminderDate) &&
        lastReminderDate == today) {
        return;
    }

    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"LastSupportReminderDate", today);
    ShowTrayNotification(T(TxtSupportReminderTitle), T(TxtSupportReminderBody),
                         NotificationActionSupportReminder);
}

void MaybeShowHdrCalibrationReminder() {
    if (g_lastHdrTargetCount <= 0 || !g_mainWindow) return;

    DWORD shown = 0;
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"HdrCalibrationHintShown", &shown) && shown != 0) {
        return;
    }

    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"HdrCalibrationHintShown", 1);
    ShowTrayNotification(T(TxtHdrCalibrationTitle), T(TxtHdrCalibrationBody),
                         NotificationActionHdrCalibration);
}

void StopBrightnessTransition() {
    if (g_mainWindow) KillTimer(g_mainWindow, kTransitionTimer);
    g_transitionActive = false;
    g_transitionManualCorrection = false;
}

void ContinueBrightnessTransition() {
    if (!g_transitionActive) return;

    ApplyResult result = ApplySdrLevelStep(g_transitionTargetLevel, true, g_lastKnownTargetLevel,
                                           kTransitionStepLevel);
    NightDecision decision;
    decision.night = g_transitionNight;
    decision.source = g_transitionSource;

    if (!result.ok) {
        StopBrightnessTransition();
        g_lastHdrTargetCount = result.targetCount;
        g_lastHdrSuccessCount = result.successCount;
        MaybeShowHdrCalibrationReminder();
        g_status = BuildStatusText(decision, g_transitionTargetBrightness, result);
        UpdateTrayTip();
        if (g_settingsWindow) {
            InvalidateRect(g_settingsWindow, NULL, FALSE);
        }
        return;
    }

    if (result.changed && result.appliedLevel != 0) {
        g_lastKnownTargetLevel = result.appliedLevel;
    }

    if (g_settingsWindow) {
        InvalidateRect(g_settingsWindow, NULL, FALSE);
    }

    if (result.complete) {
        StopBrightnessTransition();
        g_lastAppliedBrightness = g_transitionTargetBrightness;
        g_lastDecisionNight = g_transitionNight;
        g_lastKnownTargetLevel = g_transitionTargetLevel;
        g_lastHdrTargetCount = result.targetCount;
        g_lastHdrSuccessCount = result.successCount;
        MaybeShowHdrCalibrationReminder();
        g_status = BuildStatusText(decision, g_transitionTargetBrightness, result);
        UpdateTrayTip();
    }
}

void BeginBrightnessTransition(const NightDecision& decision, int brightness, bool manualCorrection) {
    g_transitionTargetBrightness = brightness;
    g_transitionTargetLevel = BrightnessPercentToSdrLevel(brightness);
    g_transitionNight = decision.night;
    g_transitionSource = decision.source;
    g_transitionManualCorrection = manualCorrection;
    g_transitionActive = true;

    if (manualCorrection) {
        NotifyManualCorrection(brightness);
    }

    if (g_mainWindow) SetTimer(g_mainWindow, kTransitionTimer, kTransitionMs, NULL);
    ContinueBrightnessTransition();
}

void ApplyCurrentBrightness(bool force) {
    if (g_settingsPreviewActive) return;

    LoadConfig();
    if (!g_brightnessConfigReady) {
        StopBrightnessTransition();
        return;
    }
    NightDecision decision = DecideNight();
    int brightness = decision.night ? g_config.nightBrightness : g_config.dayBrightness;
    UINT32 targetLevel = BrightnessPercentToSdrLevel(brightness);

    if (g_transitionActive) {
        if (g_transitionTargetBrightness != brightness ||
            g_transitionTargetLevel != targetLevel ||
            g_transitionNight != decision.night ||
            g_transitionSource != decision.source) {
            g_transitionTargetBrightness = brightness;
            g_transitionTargetLevel = targetLevel;
            g_transitionNight = decision.night;
            g_transitionSource = decision.source;
        }
        return;
    }

    ApplyResult check = CheckSdrBrightness(brightness);
    bool targetChanged =
        g_lastAppliedBrightness != brightness ||
        g_lastDecisionNight != decision.night ||
        g_lastKnownTargetLevel != targetLevel;

    if (check.ok && check.complete) {
        g_lastAppliedBrightness = brightness;
        g_lastDecisionNight = decision.night;
        g_lastKnownTargetLevel = targetLevel;
        g_lastHdrTargetCount = check.targetCount;
        g_lastHdrSuccessCount = check.successCount;
        MaybeShowHdrCalibrationReminder();
        g_status = BuildStatusText(decision, brightness, check);
        UpdateTrayTip();
        return;
    }

    bool manualCorrection = !force && !targetChanged && check.targetCount > 0 && !check.complete;
    if (manualCorrection && !g_config.autoRestoreManualChanges) {
        g_lastHdrTargetCount = check.targetCount;
        g_lastHdrSuccessCount = check.successCount;
        MaybeShowHdrCalibrationReminder();
        g_status = T(TxtManualRestoreOff);
        UpdateTrayTip();
        return;
    }
    BeginBrightnessTransition(decision, brightness, manualCorrection);
    UpdateTrayTip();
}

void ClearSettingsBrightnessPreview() {
    g_settingsPreviewActive = false;
    g_settingsPreviewBrightness = -1;
    g_settingsPreviewNight = false;
}

void RestoreSettingsBrightnessPreviewIfNeeded() {
    if (!g_settingsPreviewActive) return;
    ClearSettingsBrightnessPreview();
    if (g_mainWindow) {
        ApplyCurrentBrightness(true);
    }
}

void ApplySettingsBrightnessPreview(HWND hwnd, int id, int brightness) {
    bool previewNight = id == kIdNightBrightness;
    brightness = ClampInt(brightness, 0, 100);
    if (g_settingsPreviewActive &&
        g_settingsPreviewBrightness == brightness &&
        g_settingsPreviewNight == previewNight) {
        return;
    }

    StopBrightnessTransition();

    UINT32 targetLevel = BrightnessPercentToSdrLevel(brightness);
    ApplyResult result = ApplySdrLevelStep(targetLevel, false, g_lastKnownTargetLevel,
                                           kTransitionStepLevel);

    g_settingsPreviewActive = true;
    g_settingsPreviewBrightness = brightness;
    g_settingsPreviewNight = previewNight;
    if (result.ok) {
        g_lastAppliedBrightness = brightness;
        g_lastDecisionNight = previewNight;
        g_lastKnownTargetLevel = targetLevel;
    }
    g_lastHdrTargetCount = result.targetCount;
    g_lastHdrSuccessCount = result.successCount;

    NightDecision decision;
    decision.night = previewNight;
    decision.source = T(TxtSourcePreview);
    g_status = BuildStatusText(decision, brightness, result);
    UpdateTrayTip();
    if (hwnd) {
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

void BeginSettingsBrightnessPreviewTransition(HWND hwnd, int id, int brightness) {
    bool previewNight = id == kIdNightBrightness;
    brightness = ClampInt(brightness, 0, 100);
    UINT32 targetLevel = BrightnessPercentToSdrLevel(brightness);
    if (g_settingsPreviewActive &&
        g_settingsPreviewBrightness == brightness &&
        g_settingsPreviewNight == previewNight &&
        g_transitionActive &&
        g_transitionTargetLevel == targetLevel) {
        return;
    }

    g_settingsPreviewActive = true;
    g_settingsPreviewBrightness = brightness;
    g_settingsPreviewNight = previewNight;

    NightDecision decision;
    decision.night = previewNight;
    decision.source = T(TxtSourcePreview);

    ApplyResult check = CheckSdrBrightness(brightness);
    g_status = BuildStatusText(decision, brightness, check);
    BeginBrightnessTransition(decision, brightness, false);
    UpdateTrayTip();
    if (hwnd) {
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

int SettingsDraftBrightnessForMode(bool night) {
    return night ? g_settingsDraft.nightBrightness : g_settingsDraft.dayBrightness;
}

int ConfigBrightnessForMode(const Config& config, bool night) {
    return night ? config.nightBrightness : config.dayBrightness;
}

int BrightnessIdForMode(bool night) {
    return night ? kIdNightBrightness : kIdDayBrightness;
}

bool SettingsBrightnessIdMatchesCurrentMode(int id) {
    NightDecision decision = DecideNight();
    return id == BrightnessIdForMode(decision.night);
}

Config SettingsSavedBaseline() {
    Config baseline = g_config;
    if (!CanFollowWindowsNightLight()) {
        baseline.followNightLight = false;
    }
    return baseline;
}

bool ConfigsEqual(const Config& a, const Config& b) {
    return a.dayBrightness == b.dayBrightness &&
           a.nightBrightness == b.nightBrightness &&
           a.followNightLight == b.followNightLight &&
           a.autoRestoreManualChanges == b.autoRestoreManualChanges &&
           a.startWithWindows == b.startWithWindows &&
           a.language == b.language &&
           a.nightStartHour == b.nightStartHour &&
           a.nightStartMinute == b.nightStartMinute &&
           a.dayStartHour == b.dayStartHour &&
           a.dayStartMinute == b.dayStartMinute;
}

bool SettingsDraftHasUnsavedChanges() {
    if (!g_settingsDraftActive) return false;
    return !ConfigsEqual(g_settingsDraft, SettingsSavedBaseline());
}

void RestoreCurrentModeAfterInactivePreview(HWND hwnd) {
    NightDecision decision = DecideNight();
    int draftBrightness = SettingsDraftBrightnessForMode(decision.night);
    int savedBrightness = ConfigBrightnessForMode(g_config, decision.night);
    if (draftBrightness != savedBrightness) {
        BeginSettingsBrightnessPreviewTransition(hwnd, BrightnessIdForMode(decision.night), draftBrightness);
        return;
    }

    RestoreSettingsBrightnessPreviewIfNeeded();
    if (hwnd) {
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

bool IsCachedStatusText(TextId id) {
    return IsLocalizedTextValue(id, g_status);
}

void RefreshStatusTextForCurrentLanguage() {
    if (g_status.empty() || IsCachedStatusText(TxtStarting)) {
        g_status = T(TxtStarting);
        return;
    }

    if (IsCachedStatusText(TxtManualRestoreOff)) {
        g_status = T(TxtManualRestoreOff);
        return;
    }

    if (g_settingsPreviewActive && g_settingsPreviewBrightness >= 0) {
        NightDecision decision;
        decision.night = g_settingsPreviewNight;
        decision.source = T(TxtSourcePreview);
        ApplyResult check = CheckSdrBrightness(g_settingsPreviewBrightness);
        g_status = BuildStatusText(decision, g_settingsPreviewBrightness, check);
        return;
    }

    if (g_transitionActive && g_transitionTargetBrightness >= 0) {
        g_status = F(TxtTrayRestoringBrightness, {PercentLabel(g_transitionTargetBrightness)});
        return;
    }

    NightDecision decision = DecideNight();
    int brightness = decision.night ? g_config.nightBrightness : g_config.dayBrightness;
    ApplyResult check = CheckSdrBrightness(brightness);
    g_status = BuildStatusText(decision, brightness, check);
}

void AddTrayIcon(HWND hwnd) {
    g_trayIcon.Add(g_instance, hwnd, kTrayMessage, IDI_APPICON, T(TxtDisplayName));
}

void RemoveTrayIcon() {
    g_trayIcon.Remove();
}

void AppendMenuText(HMENU menu, UINT flags, UINT_PTR id, const std::wstring& text) {
    AppendMenuW(menu, flags, id, text.c_str());
}

void ReloadUiTheme() {
    g_theme = ui_theme::BuildTheme();
    ui_theme::ApplySystemMenuTheme(g_theme.dark);
    if (g_windowBrush) {
        DeleteObject(g_windowBrush);
        g_windowBrush = NULL;
    }
    if (g_panelBrush) {
        DeleteObject(g_panelBrush);
        g_panelBrush = NULL;
    }
    if (g_editBrush) {
        DeleteObject(g_editBrush);
        g_editBrush = NULL;
    }
}

void RefreshUiDpi(HWND hwnd) {
    if (ui_dpi::RefreshForWindow(hwnd)) {
        CleanupFontResources();
    }
}

void RefreshUiDpiForNewTopLevelWindow(HWND owner) {
    if (ui_dpi::RefreshForNewTopLevelWindow(owner)) {
        CleanupFontResources();
    }
}

int Ui(int value) {
    return ui_dpi::Scale(value);
}

int FromUi(int value) {
    return ui_dpi::Unscale(value);
}

RECT UiBox(int x, int y, int width, int height) {
    return ui_dpi::Box(x, y, width, height);
}

bool PtInUiBox(POINT pt, int x, int y, int width, int height) {
    return ui_dpi::PtInBox(pt, x, y, width, height);
}

POINT SettingsViewportPoint(POINT pt) {
    pt.y -= Ui(kSettingsTitleBarHeight);
    return pt;
}

POINT SettingsContentPoint(POINT pt) {
    pt = SettingsViewportPoint(pt);
    pt.y += Ui(g_settingsScrollY);
    return pt;
}

bool IsSettingsTitleControl(int control) {
    return control == HoverTitleHelp ||
           control == HoverTitleGithub ||
           control == HoverTitleMinimize ||
           control == HoverTitleClose;
}

int SettingsTitleButtonX(int control) {
    switch (control) {
    case HoverTitleClose:
        return kSettingsClientWidth - 46;
    case HoverTitleMinimize:
        return kSettingsClientWidth - 90;
    case HoverTitleGithub:
        return kSettingsClientWidth - 134;
    case HoverTitleHelp:
        return kSettingsClientWidth - 178;
    default:
        return 0;
    }
}

RECT SettingsTitleButtonBox(int control) {
    return UiBox(SettingsTitleButtonX(control), 5, 38, 34);
}

int HitTestSettingsTitleControl(POINT pt) {
    if (pt.y < 0 || pt.y >= Ui(kSettingsTitleBarHeight)) return HoverNone;
    static const int controls[] = {
        HoverTitleHelp,
        HoverTitleGithub,
        HoverTitleMinimize,
        HoverTitleClose
    };
    for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
        RECT rect = SettingsTitleButtonBox(controls[i]);
        if (PtInRect(&rect, pt)) return controls[i];
    }
    return HoverNone;
}

int SettingsContentClientHeight(HWND hwnd) {
    RECT client = {};
    GetClientRect(hwnd, &client);
    return std::max(1, FromUi(client.bottom - client.top) - kSettingsTitleBarHeight);
}

RECT SettingsDialogBox(HWND hwnd) {
    int visibleHeight = SettingsContentClientHeight(hwnd);
    const int w = 430;
    const int h = 220;
    int x = (kSettingsClientWidth - w) / 2;
    int y = std::max(24, (visibleHeight - h) / 2);
    return UiBox(x, y, w, h);
}

RECT SettingsDialogCloseBox(HWND hwnd) {
    RECT dialog = SettingsDialogBox(hwnd);
    RECT rect = {dialog.right - Ui(46), dialog.top + Ui(12), dialog.right - Ui(14), dialog.top + Ui(44)};
    return rect;
}

RECT SettingsDialogOkBox(HWND hwnd) {
    RECT dialog = SettingsDialogBox(hwnd);
    RECT rect = {dialog.right - Ui(102), dialog.bottom - Ui(54), dialog.right - Ui(24), dialog.bottom - Ui(20)};
    return rect;
}

RECT SettingsDialogLinkBox(HWND hwnd) {
    RECT dialog = SettingsDialogBox(hwnd);
    RECT rect = {dialog.left + Ui(74), dialog.top + Ui(128), dialog.right - Ui(28), dialog.top + Ui(152)};
    return rect;
}

RECT HotkeyDialogScreenshotBox(HWND hwnd) {
    RECT dialog = SettingsDialogBox(hwnd);
    return UiBox(FromUi(dialog.right) - 188, FromUi(dialog.top) + 72, 150, 30);
}

RECT HotkeyDialogFullscreenBox(HWND hwnd) {
    RECT dialog = SettingsDialogBox(hwnd);
    return UiBox(FromUi(dialog.right) - 188, FromUi(dialog.top) + 118, 150, 30);
}

bool IsHover(int control) {
    return g_hoverControl == control;
}

int ApproachInt(int current, int target, int step) {
    if (current < target) return std::min(target, current + step);
    if (current > target) return std::max(target, current - step);
    return current;
}

bool AnimateValue(int* value, int target, int step) {
    int next = ApproachInt(*value, target, step);
    if (next == *value) return false;
    *value = next;
    return true;
}

void ArmSettingsAnimationTimer(HWND hwnd) {
    if (hwnd) SetTimer(hwnd, kSettingsAnimationTimer, kSettingsAnimationMs, NULL);
}

int ControlAnim(int control) {
    if (control <= HoverNone || control >= kAnimationSlotCount) return 0;
    return g_controlAnim[control];
}

int InteractionPercent(int control) {
    return ClampInt(ControlAnim(control) / 10, 0, 100);
}

bool AnimateControlSlot(int control) {
    if (control <= HoverNone || control >= kAnimationSlotCount) return false;
    bool active = g_hoverControl == control || g_pressedControl == control ||
                  g_supportPressedControl == control;
    return AnimateValue(&g_controlAnim[control], active ? 1000 : 0, active ? 180 : 140);
}

bool UpdateSettingsAnimations(HWND hwnd) {
    bool changed = false;
    changed |= AnimateValue(&g_settingsWindowAnim, 1000, 120);
    changed |= AnimateValue(&g_settingsDialogAnim, (g_settingsInfoDialogOpen || g_hotkeyDialogOpen) ? 1000 : 0, 150);
    changed |= AnimateValue(&g_settingsDropdownAnim, g_languageDropdownOpen ? 1000 : 0, 180);

    static const int controls[] = {
        HoverLanguageButton,
        HoverSwitchBase,
        HoverSwitchBase + 1,
        HoverDaySlider,
        HoverNightSlider,
        HoverAutoRestore,
        HoverStartup,
        HoverNightMinus,
        HoverNightPlus,
        HoverDayMinus,
        HoverDayPlus,
        HoverOk,
        HoverApply,
        HoverCancel,
        HoverSupport,
        HoverSupporterBadge,
        HoverHdrCalibration,
        HoverHdrCalibrationDismiss,
        HoverTitleHelp,
        HoverTitleGithub,
        HoverTitleMinimize,
        HoverTitleClose,
        HoverDialogOk,
        HoverDialogClose,
        HoverDialogLink,
        HoverHotkeySettings,
        HoverNotifyOk,
        HoverNotifySettings,
        HoverSupportDonate,
        HoverSupportActivate,
        HoverSupportCode,
        HoverScreenshotHotkey,
        HoverFullscreenHotkey
    };

    for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
        changed |= AnimateControlSlot(controls[i]);
    }
    for (int i = 0; i < LanguageOptionCount(); ++i) {
        changed |= AnimateControlSlot(HoverLanguageOptionBase + i);
    }

    if (changed && hwnd) {
        InvalidateRect(hwnd, NULL, FALSE);
    }

    bool supportLoop = IsSupportFeatureAvailable() &&
                       hwnd == g_settingsWindow && !HasSupporterBadge() &&
                       !g_settingsInfoDialogOpen && !g_hotkeyDialogOpen && !IsIconic(hwnd);
    if (supportLoop) {
        g_supportButtonAnim = (g_supportButtonAnim + 1) % 120;
        RECT supportRect = UiBox(SupportButtonLeft(NULL) - 4,
                                 kSettingsTitleBarHeight + kStoreSupportButtonY - 4,
                                 kStoreSupportButtonW + 8,
                                 kStoreSupportButtonH + 8);
        InvalidateRect(hwnd, &supportRect, FALSE);
    }

    return changed || supportLoop;
}

int HoverSegment(int base, int count) {
    int segment = g_hoverControl - base;
    return (segment >= 0 && segment < count) ? segment : -1;
}

int SliderValueFromPoint(POINT pt, int x, int width) {
    int left = Ui(x);
    int w = std::max(1, Ui(width));
    return ClampInt(MulDiv(pt.x - left, 100, w), 0, 100);
}

HFONT CreateUiFont(int pointSize, int weight) {
    int height = ui_dpi::FontHeightForPointSize(pointSize);
    const wchar_t* primaryFace = L"Segoe UI Variable Text";
    switch (CurrentUiLanguage()) {
    case LangChinese:
        primaryFace = L"Microsoft YaHei UI";
        break;
    case LangChineseTraditional:
        primaryFace = L"Microsoft JhengHei UI";
        break;
    case LangKorean:
        primaryFace = L"Malgun Gothic";
        break;
    case LangJapanese:
        primaryFace = L"Yu Gothic UI";
        break;
    case LangRussian:
    case LangEnglish:
    default:
        primaryFace = L"Segoe UI Variable Text";
        break;
    }
    const wchar_t* fallbackFace = L"Segoe UI";
    HFONT font = CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, primaryFace);
    if (!font) {
        font = CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, fallbackFace);
    }
    return font;
}

bool EnsureGdiplus() {
    return ui_gdiplus::EnsureStarted();
}

void ShutdownGdiplus() {
    ui_gdiplus::Shutdown();
}

void EnsureUiResources() {
    EnsureGdiplus();
    if (g_theme.window == 0 && g_theme.card == 0) {
        g_theme = ui_theme::BuildTheme();
    }
    if (!g_uiFont) g_uiFont = CreateUiFont(10, FW_NORMAL);
    if (!g_smallFont) g_smallFont = CreateUiFont(9, FW_NORMAL);
    if (!g_titleFont) g_titleFont = CreateUiFont(18, FW_SEMIBOLD);
    if (!g_sectionFont) g_sectionFont = CreateUiFont(11, FW_SEMIBOLD);
    if (!g_heroFont) g_heroFont = CreateUiFont(21, FW_SEMIBOLD);
    if (!g_windowBrush) g_windowBrush = CreateSolidBrush(g_theme.window);
    if (!g_panelBrush) g_panelBrush = CreateSolidBrush(g_theme.card);
    if (!g_editBrush) g_editBrush = CreateSolidBrush(g_theme.control);
}

void CleanupFontResources() {
    if (g_uiFont) DeleteObject(g_uiFont);
    if (g_smallFont) DeleteObject(g_smallFont);
    if (g_titleFont) DeleteObject(g_titleFont);
    if (g_sectionFont) DeleteObject(g_sectionFont);
    if (g_heroFont) DeleteObject(g_heroFont);
    g_uiFont = NULL;
    g_smallFont = NULL;
    g_titleFont = NULL;
    g_sectionFont = NULL;
    g_heroFont = NULL;
}

void CleanupUiResources() {
    CleanupFontResources();
    if (g_windowBrush) DeleteObject(g_windowBrush);
    if (g_panelBrush) DeleteObject(g_panelBrush);
    if (g_editBrush) DeleteObject(g_editBrush);
    g_windowBrush = NULL;
    g_panelBrush = NULL;
    g_editBrush = NULL;
}

void ApplyModernWindowFrame(HWND hwnd) {
    ui_window::ApplyModernFrame(hwnd, g_theme.dark, g_theme.window, g_theme.titleText);
}

void AddRoundedRectPath(Gdiplus::GraphicsPath* path, Gdiplus::REAL x, Gdiplus::REAL y,
                        Gdiplus::REAL w, Gdiplus::REAL h, Gdiplus::REAL radius) {
    ui_window::AddRoundedRectPath(path, x, y, w, h, radius);
}

void ShowTrayMenu(HWND hwnd) {
    ReloadUiTheme();

    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    RefreshStatusTextForCurrentLanguage();
    std::wstring status = g_status.empty() ? T(TxtStarting) : g_status;
    AppendMenuText(menu, MF_STRING | MF_DISABLED, 0, status);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, kMenuApply, T(TxtMenuApply));
    AppendMenuW(menu, MF_STRING, kMenuSettings, T(TxtMenuSettings));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    if (IsStartupFeatureAvailable()) {
        AppendMenuW(menu, MF_STRING | (IsStartupEnabled() ? MF_CHECKED : 0), kMenuStartup, T(TxtMenuStartup));
    }
    AppendMenuW(menu, MF_STRING, kMenuDisplaySettings, T(TxtMenuDisplaySettings));
    AppendMenuW(menu, MF_STRING, kMenuNightLightSettings, T(TxtMenuNightLightSettings));
    AppendMenuW(menu, MF_STRING, kMenuHdrCalibration, T(TxtMenuHdrCalibration));
    AppendMenuW(menu, MF_STRING, kMenuHdrScreenshot, T(TxtMenuHdrScreenshot));
    if (IsSupportFeatureAvailable()) {
        AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(menu, MF_STRING, kMenuSupport, T(TxtMenuSupport));
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, kMenuExit, T(TxtMenuExit));

    POINT pt = {};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);
}

bool ReadSettingsWindow(HWND hwnd, Config* config) {
    (void)hwnd;
    *config = g_settingsDraft;
    return true;
}

void ApplySettingsFromWindow(HWND hwnd, bool closeWindow) {
    Config next;
    if (!ReadSettingsWindow(hwnd, &next)) return;
    ClearSettingsBrightnessPreview();
    g_config = next;
    g_brightnessConfigReady = true;
    SaveConfig();
    PostMessageW(g_mainWindow, kApplyMessage, TRUE, 0);
    if (closeWindow) {
        DestroyWindow(hwnd);
    } else {
        UpdateSettingsWindowTitle(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateTrayTip();
    }
}

void CenterWindow(HWND hwnd, int width, int height) {
    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int x = work.left + ((work.right - work.left) - width) / 2;
    int y = work.top + ((work.bottom - work.top) - height) / 2;
    SetWindowPos(hwnd, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

SIZE WindowSizeForClient(DWORD style, DWORD exStyle, int clientWidth, int clientHeight) {
    RECT rect = {0, 0, clientWidth, clientHeight};
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    SIZE size = {rect.right - rect.left, rect.bottom - rect.top};
    return size;
}

int SettingsVisibleClientHeight(int contentHeight, DWORD style, DWORD exStyle) {
    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int workHeight = work.bottom - work.top;
    int safeClientPx = std::max(Ui(kSettingsMinVisibleClientHeight),
                                workHeight - Ui(96 + kSettingsTitleBarHeight));
    int safeClientLogical = FromUi(safeClientPx);
    int maxVisible = std::max(kSettingsMinVisibleClientHeight, safeClientLogical);

    SIZE maxWindow = WindowSizeForClient(style, exStyle, Ui(kSettingsClientWidth),
                                         Ui(maxVisible + kSettingsTitleBarHeight));
    while (maxVisible > kSettingsMinVisibleClientHeight && maxWindow.cy > workHeight - Ui(24)) {
        maxVisible -= 20;
        maxWindow = WindowSizeForClient(style, exStyle, Ui(kSettingsClientWidth),
                                        Ui(maxVisible + kSettingsTitleBarHeight));
    }

    return std::min(contentHeight, maxVisible);
}

struct SettingsLayout {
    int clientHeight;
    int cardX;
    int cardW;
    int headerIconX;
    int headerIconY;
    int headerTitleX;
    int headerTitleY;
    int headerSubtitleY;
    int languageTop;
    int languageH;
    int languageSegmentX;
    int languageSegmentY;
    int languageSegmentW;
    int languageSegmentH;
    int heroTop;
    int heroH;
    int brightnessX;
    int brightnessW;
    int brightnessTop;
    int brightnessH;
    int brightnessRow1;
    int brightnessRow2;
    int switchX;
    int switchW;
    int switchTop;
    int switchH;
    int switchModeX;
    int switchModeY;
    int switchModeW;
    int switchModeH;
    int switchHintY;
    int switchNightY;
    int switchDayY;
    int appearanceX;
    int appearanceW;
    int appearanceTop;
    int appearanceH;
    int appearanceRow1;
    int behaviorX;
    int behaviorW;
    int behaviorTop;
    int behaviorH;
    int behaviorRow1;
    int behaviorRow2;
    int hotkeyTop;
    int hotkeyH;
    int hotkeyRow1;
    int hotkeyRow2;
    int footerY;
};

bool ShouldShowHdrCalibrationCallout() {
    return g_lastHdrTargetCount > 0 && !g_hdrCalibrationCalloutDismissed;
}

int HdrCalibrationCalloutY(const SettingsLayout& layout) {
    return layout.heroTop + 100;
}

RECT HdrCalibrationLinkBox(const SettingsLayout& layout) {
    int dismissX = layout.cardX + layout.cardW - kSettingsCardPadding - 24;
    return UiBox(dismissX - 132, HdrCalibrationCalloutY(layout), 120, 28);
}

RECT HdrCalibrationDismissBox(const SettingsLayout& layout) {
    return UiBox(layout.cardX + layout.cardW - kSettingsCardPadding - 24,
                 HdrCalibrationCalloutY(layout) + 2, 24, 24);
}

SettingsLayout BuildSettingsLayout(bool useSystemSwitching) {
    SettingsLayout layout = {};
    const int cardGap = 8;

    layout.cardX = 28;
    layout.cardW = 584;

    layout.headerIconX = 28;
    layout.headerIconY = 22;
    layout.headerTitleX = 78;
    layout.headerTitleY = 18;
    layout.headerSubtitleY = 48;

    layout.heroTop = 82;
    layout.heroH = ShouldShowHdrCalibrationCallout() ? 140 : 104;

    layout.brightnessTop = layout.heroTop + layout.heroH + cardGap;
    layout.brightnessX = layout.cardX;
    layout.brightnessW = layout.cardW;
    layout.brightnessH = 218;
    layout.brightnessRow1 = layout.brightnessTop + 154;
    layout.brightnessRow2 = layout.brightnessRow1 + 36;

    layout.switchTop = layout.brightnessTop + layout.brightnessH + cardGap;
    layout.switchX = layout.cardX;
    layout.switchW = layout.cardW;
    layout.switchH = useSystemSwitching ? 88 : 124;
    layout.switchModeW = kSettingsRightControlWidth;
    layout.switchModeH = 32;
    layout.switchModeX = layout.cardX + layout.cardW - kSettingsCardPadding - layout.switchModeW;
    layout.switchModeY = layout.switchTop + 16;
    layout.switchHintY = layout.switchTop + 56;
    layout.switchNightY = layout.switchTop + 56;
    layout.switchDayY = layout.switchNightY + 36;

    layout.appearanceTop = layout.switchTop + layout.switchH + cardGap;
    layout.appearanceX = layout.cardX;
    layout.appearanceW = layout.cardW;
    layout.appearanceH = 80;
    layout.appearanceRow1 = layout.appearanceTop + 44;

    layout.languageSegmentW = kSettingsRightControlWidth;
    layout.languageSegmentH = 32;
    layout.languageSegmentX = layout.cardX + layout.cardW - kSettingsCardPadding - layout.languageSegmentW;
    layout.languageSegmentY = layout.appearanceRow1 - 2;
    layout.languageTop = layout.languageSegmentY;
    layout.languageH = layout.languageSegmentH;

    layout.behaviorTop = layout.appearanceTop + layout.appearanceH + cardGap;
    layout.behaviorX = layout.cardX;
    layout.behaviorW = layout.cardW;
    layout.behaviorH = IsStartupFeatureAvailable() ? 154 : 118;
    layout.behaviorRow1 = layout.behaviorTop + 44;
    layout.behaviorRow2 = layout.behaviorRow1 + 36;

    layout.hotkeyTop = 0;
    layout.hotkeyH = 0;
    layout.hotkeyRow1 = IsStartupFeatureAvailable() ? layout.behaviorRow2 + 36 : layout.behaviorRow1 + 36;
    layout.hotkeyRow2 = layout.hotkeyRow1;

    layout.footerY = layout.behaviorTop + layout.behaviorH + 16;
    layout.clientHeight = std::max(kSettingsClientHeight, layout.footerY + kSettingsFooterAreaHeight);
    return layout;
}

bool SettingsDraftUsesSystemSwitching() {
    return g_settingsDraft.followNightLight && CanFollowWindowsNightLight();
}

void UpdateHdrPreviewWindow(HWND hwnd);

void ResizeSettingsWindowToLayout(HWND hwnd) {
    SettingsLayout layout = BuildSettingsLayout(SettingsDraftUsesSystemSwitching());
    DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    int visibleHeight = SettingsVisibleClientHeight(layout.clientHeight, style, exStyle);
    g_settingsScrollY = ClampInt(g_settingsScrollY, 0, std::max(0, layout.footerY - (visibleHeight - kSettingsFooterAreaHeight)));
    SIZE size = WindowSizeForClient(style, exStyle, Ui(kSettingsClientWidth),
                                    Ui(visibleHeight + kSettingsTitleBarHeight));
    SetWindowPos(hwnd, NULL, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    UpdateHdrPreviewWindow(hwnd);
}

void DismissHdrCalibrationCallout(HWND hwnd) {
    g_hdrCalibrationCalloutDismissed = true;
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"HdrCalibrationCalloutDismissed", 1);
    if (hwnd) {
        ResizeSettingsWindowToLayout(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

int SettingsVisibleClientHeight(HWND hwnd) {
    return SettingsContentClientHeight(hwnd);
}

int SettingsScrollableViewportHeight(HWND hwnd) {
    return std::max(1, SettingsVisibleClientHeight(hwnd) - kSettingsFooterAreaHeight);
}

int SettingsMaxScroll(HWND hwnd) {
    SettingsLayout layout = BuildSettingsLayout(SettingsDraftUsesSystemSwitching());
    return std::max(0, layout.footerY - SettingsScrollableViewportHeight(hwnd));
}

void SetSettingsScrollY(HWND hwnd, int scrollY) {
    int maxScroll = SettingsMaxScroll(hwnd);
    int next = ClampInt(scrollY, 0, maxScroll);
    if (next == g_settingsScrollY) return;
    g_settingsScrollY = next;
    UpdateHdrPreviewWindow(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

int BrightnessSliderX(const SettingsLayout& layout) {
    (void)layout;
    return 210;
}

int BrightnessSliderWidth(const SettingsLayout&) {
    return 300;
}

int BrightnessValueX(const SettingsLayout& layout) {
    return layout.cardX + layout.cardW - kSettingsCardPadding - 56;
}

int TimeStepperMinusX(const SettingsLayout& layout) {
    return layout.cardX + layout.cardW - kSettingsCardPadding - 166;
}

int TimeStepperValueX(const SettingsLayout& layout) {
    return layout.cardX + layout.cardW - kSettingsCardPadding - 122;
}

int TimeStepperPlusX(const SettingsLayout& layout) {
    return layout.cardX + layout.cardW - kSettingsCardPadding - 28;
}

int LanguageDropdownWidth() {
    return 160;
}

int LanguageDropdownHeight() {
    return 12 + 34 * LanguageOptionCount();
}

int LanguageDropdownY(const SettingsLayout& layout) {
    int h = LanguageDropdownHeight();
    int downY = layout.languageSegmentY + layout.languageSegmentH + 6;
    int upY = layout.languageSegmentY - 6 - h;
    int viewportH = g_settingsWindow ? SettingsScrollableViewportHeight(g_settingsWindow)
                                     : kSettingsMinVisibleClientHeight - kSettingsFooterAreaHeight;
    int visibleTop = g_settingsScrollY;
    int visibleBottom = visibleTop + viewportH;

    if (downY + h > visibleBottom - 8) {
        if (upY >= visibleTop + 8) {
            return upY;
        }
        return std::max(visibleTop + 8, visibleBottom - h - 8);
    }
    return downY;
}

RECT LanguageDropdownBox(const SettingsLayout& layout) {
    int w = LanguageDropdownWidth();
    int x = layout.languageSegmentX + layout.languageSegmentW - w;
    int h = LanguageDropdownHeight();
    return UiBox(x, LanguageDropdownY(layout), w, h);
}

void BrightnessPreviewLayout(const SettingsLayout& layout, int* leftX, int* topY,
                             int* imageW, int* imageH, int* gap) {
    const int previewGap = 52;
    const int previewW = (layout.cardW - kSettingsCardPadding * 2 - previewGap) / 2;
    if (leftX) *leftX = layout.cardX + kSettingsCardPadding;
    if (topY) *topY = layout.brightnessTop + 48;
    if (imageW) *imageW = previewW;
    if (imageH) *imageH = 68;
    if (gap) *gap = previewGap;
}

std::wstring HotkeyText(UINT mod, UINT vk) {
    return app_hotkeys::Format(mod, vk, T(TxtHotkeyNone));
}

int LanguageDropdownOptionFromPoint(POINT pt, const SettingsLayout& layout) {
    RECT box = LanguageDropdownBox(layout);
    if (!PtInRect(&box, pt)) return -1;

    const int pad = Ui(6);
    const int optionH = Ui(34);
    int localY = pt.y - box.top - pad;
    if (localY < 0) return -1;

    int option = localY / std::max(1, optionH);
    if (option < 0 || option >= LanguageOptionCount()) return -1;
    if (localY % std::max(1, optionH) >= optionH) return -1;
    return option;
}

int HitTestSettingsControl(POINT pt) {
    bool canFollow = CanFollowWindowsNightLight();
    bool useSystem = g_settingsDraft.followNightLight && canFollow;
    SettingsLayout layout = BuildSettingsLayout(useSystem);

    if (g_languageDropdownOpen) {
        int option = LanguageDropdownOptionFromPoint(pt, layout);
        if (option >= 0) return HoverLanguageOptionBase + option;
    }

    if (PtInUiBox(pt, layout.languageSegmentX, layout.languageSegmentY, layout.languageSegmentW, layout.languageSegmentH)) {
        return HoverLanguageButton;
    }

    int sliderX = BrightnessSliderX(layout);
    int sliderW = BrightnessSliderWidth(layout);
    if (PtInUiBox(pt, sliderX - 20, layout.brightnessRow1 - 22, sliderW + 40, 40)) return HoverDaySlider;
    if (PtInUiBox(pt, sliderX - 20, layout.brightnessRow2 - 22, sliderW + 40, 40)) return HoverNightSlider;

    if (PtInUiBox(pt, layout.switchModeX, layout.switchModeY, layout.switchModeW, layout.switchModeH)) {
        int localX = pt.x - Ui(layout.switchModeX);
        int segment = ClampInt(localX / std::max(1, Ui(layout.switchModeW / 2)), 0, 1);
        return HoverSwitchBase + segment;
    }

    if (ShouldShowHdrCalibrationCallout()) {
        RECT linkRect = HdrCalibrationLinkBox(layout);
        if (PtInRect(&linkRect, pt)) return HoverHdrCalibration;
        RECT dismissRect = HdrCalibrationDismissBox(layout);
        if (PtInRect(&dismissRect, pt)) return HoverHdrCalibrationDismiss;
    }

    if (PtInUiBox(pt, layout.cardX + 12, layout.behaviorRow1 - 5, layout.cardW - 24, 36)) return HoverAutoRestore;
    if (IsStartupFeatureAvailable() &&
        PtInUiBox(pt, layout.cardX + 12, layout.behaviorRow2 - 5, layout.cardW - 24, 36)) return HoverStartup;
    if (PtInUiBox(pt, layout.languageSegmentX, layout.hotkeyRow1 - 4,
                  layout.languageSegmentW, 32)) return HoverHotkeySettings;

    if (!useSystem && PtInUiBox(pt, TimeStepperMinusX(layout), layout.switchNightY - 4, 28, 28)) return HoverNightMinus;
    if (!useSystem && PtInUiBox(pt, TimeStepperPlusX(layout), layout.switchNightY - 4, 28, 28)) return HoverNightPlus;
    if (!useSystem && PtInUiBox(pt, TimeStepperMinusX(layout), layout.switchDayY - 4, 28, 28)) return HoverDayMinus;
    if (!useSystem && PtInUiBox(pt, TimeStepperPlusX(layout), layout.switchDayY - 4, 28, 28)) return HoverDayPlus;

    return HoverNone;
}

int HitTestSettingsFooterControl(HWND hwnd, POINT pt) {
    pt = SettingsViewportPoint(pt);
    if (pt.y < 0) return HoverNone;
    int footerY = SettingsVisibleClientHeight(hwnd) - kSettingsFooterAreaHeight + 19;
    const int buttonW = 84;
    const int buttonGap = 6;
    const int buttonRight = 28 + 584 - kSettingsCardPadding;
    const int cancelX = buttonRight - buttonW;
    const int applyX = cancelX - buttonGap - buttonW;
    const int okX = applyX - buttonGap - buttonW;
    if (PtInUiBox(pt, okX, footerY, buttonW, 34)) return HoverOk;
    if (PtInUiBox(pt, applyX, footerY, buttonW, 34)) return HoverApply;
    if (PtInUiBox(pt, cancelX, footerY, buttonW, 34)) return HoverCancel;
    return HoverNone;
}

int HitTestSettingsTopControl(POINT pt) {
    if (!IsSupportFeatureAvailable()) return HoverNone;

    pt = SettingsViewportPoint(pt);
    if (pt.y < 0) return HoverNone;
    const int supportMaxW = kStoreSupportButtonW;
    const int supportX = 28 + 584 - kSettingsCardPadding - supportMaxW;
    const int supportHitW = 168;
    if (HasSupporterBadge() && PtInUiBox(pt, supportX, 28, supportMaxW, 32)) return HoverSupporterBadge;
    if (!HasSupporterBadge() &&
        PtInUiBox(pt, supportX, kStoreSupportButtonY, supportHitW, kStoreSupportButtonH)) {
        return HoverSupport;
    }
    return HoverNone;
}

int HitTestSettingsDialogControl(HWND hwnd, POINT pt) {
    if (!g_settingsInfoDialogOpen && !g_hotkeyDialogOpen) return HoverNone;
    pt = SettingsViewportPoint(pt);
    if (pt.y < 0) return HoverNone;

    RECT closeRect = SettingsDialogCloseBox(hwnd);
    if (PtInRect(&closeRect, pt)) return HoverDialogClose;

    RECT okRect = SettingsDialogOkBox(hwnd);
    if (PtInRect(&okRect, pt)) return HoverDialogOk;

    if (g_hotkeyDialogOpen) {
        RECT screenshotRect = HotkeyDialogScreenshotBox(hwnd);
        if (PtInRect(&screenshotRect, pt)) return HoverScreenshotHotkey;

        RECT fullscreenRect = HotkeyDialogFullscreenBox(hwnd);
        if (PtInRect(&fullscreenRect, pt)) return HoverFullscreenHotkey;

        return HoverNone;
    }

    RECT linkRect = SettingsDialogLinkBox(hwnd);
    if (PtInRect(&linkRect, pt)) return HoverDialogLink;

    return HoverNone;
}

void RememberSettingsMousePoint(POINT pt) {
    g_settingsMousePoint = pt;
    g_settingsMouseKnown = true;
}

void UpdateSettingsHover(HWND hwnd, POINT pt) {
    RememberSettingsMousePoint(pt);
    if (!g_trackingSettingsMouse) {
        TRACKMOUSEEVENT track = {};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = hwnd;
        TrackMouseEvent(&track);
        g_trackingSettingsMouse = true;
    }

    int nextHover = HitTestSettingsTitleControl(pt);
    if (nextHover == HoverNone) {
        nextHover = HitTestSettingsDialogControl(hwnd, pt);
    }
    bool dialogOpen = g_settingsInfoDialogOpen || g_hotkeyDialogOpen;
    if (nextHover == HoverNone && !dialogOpen) {
        nextHover = HitTestSettingsTopControl(pt);
    }
    if (nextHover == HoverNone && !dialogOpen) {
        nextHover = HitTestSettingsFooterControl(hwnd, pt);
    }
    if (nextHover == HoverNone && !dialogOpen) {
        nextHover = HitTestSettingsControl(SettingsContentPoint(pt));
    }
    if (nextHover != g_hoverControl) {
        g_hoverControl = nextHover;
        ArmSettingsAnimationTimer(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
    } else if (nextHover != HoverNone) {
        InvalidateRect(hwnd, NULL, FALSE);
    }
    SetCursor(LoadCursorW(NULL, g_hoverControl != HoverNone ? IDC_HAND : IDC_ARROW));
}

void DrawTextLine(HDC dc, const wchar_t* text, RECT rect, HFONT font, COLORREF color, UINT format) {
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, format);
    SelectObject(dc, oldFont);
}

void DrawSolidLogicalRect(HDC dc, int x, int y, int w, int h, COLORREF color) {
    RECT rect = {Ui(x), Ui(y), Ui(x) + std::max(1, Ui(w)), Ui(y + h)};
    HBRUSH brush = CreateSolidBrush(color);
    if (!brush) return;
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void DrawRoundedFill(HDC dc, int x, int y, int w, int h, int radius, COLORREF fill, COLORREF border) {
    int px = Ui(x);
    int py = Ui(y);
    int pw = Ui(w);
    int ph = Ui(h);
    int pr = Ui(radius);
    if (pw <= 0 || ph <= 0) return;

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    Gdiplus::GraphicsPath path;
    AddRoundedRectPath(&path,
                       static_cast<Gdiplus::REAL>(px) + 0.5f,
                       static_cast<Gdiplus::REAL>(py) + 0.5f,
                       static_cast<Gdiplus::REAL>(pw) - 1.0f,
                       static_cast<Gdiplus::REAL>(ph) - 1.0f,
                       static_cast<Gdiplus::REAL>(pr));

    Gdiplus::SolidBrush fillBrush(Gdiplus::Color(255, GetRValue(fill), GetGValue(fill), GetBValue(fill)));
    graphics.FillPath(&fillBrush, &path);

    Gdiplus::Pen borderPen(Gdiplus::Color(255, GetRValue(border), GetGValue(border), GetBValue(border)), 1.0f);
    graphics.DrawPath(&borderPen, &path);
}

void DrawRoundedOutline(HDC dc, int x, int y, int w, int h, int radius, COLORREF border, BYTE alpha = 255, float width = 1.0f) {
    int px = Ui(x);
    int py = Ui(y);
    int pw = Ui(w);
    int ph = Ui(h);
    int pr = Ui(radius);
    if (pw <= 0 || ph <= 0) return;

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    Gdiplus::GraphicsPath path;
    AddRoundedRectPath(&path,
                       static_cast<Gdiplus::REAL>(px) + 0.5f,
                       static_cast<Gdiplus::REAL>(py) + 0.5f,
                       static_cast<Gdiplus::REAL>(pw) - 1.0f,
                       static_cast<Gdiplus::REAL>(ph) - 1.0f,
                       static_cast<Gdiplus::REAL>(pr));
    Gdiplus::Pen borderPen(Gdiplus::Color(alpha, GetRValue(border), GetGValue(border), GetBValue(border)), width);
    graphics.DrawPath(&borderPen, &path);
}

void DrawCircleFill(HDC dc, int x, int y, int d, COLORREF fill, COLORREF border) {
    int px = Ui(x);
    int py = Ui(y);
    int pd = Ui(d);
    if (pd <= 0) return;

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    Gdiplus::SolidBrush fillBrush(Gdiplus::Color(255, GetRValue(fill), GetGValue(fill), GetBValue(fill)));
    graphics.FillEllipse(&fillBrush, static_cast<Gdiplus::REAL>(px) + 0.5f,
                         static_cast<Gdiplus::REAL>(py) + 0.5f,
                         static_cast<Gdiplus::REAL>(pd) - 1.0f,
                         static_cast<Gdiplus::REAL>(pd) - 1.0f);

    Gdiplus::Pen borderPen(Gdiplus::Color(255, GetRValue(border), GetGValue(border), GetBValue(border)), 1.0f);
    graphics.DrawEllipse(&borderPen, static_cast<Gdiplus::REAL>(px) + 0.5f,
                         static_cast<Gdiplus::REAL>(py) + 0.5f,
                         static_cast<Gdiplus::REAL>(pd) - 1.0f,
                         static_cast<Gdiplus::REAL>(pd) - 1.0f);
}

Gdiplus::Color GdiColor(COLORREF color, BYTE alpha = 255) {
    return Gdiplus::Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color));
}

void DrawRoundedAlphaFill(HDC dc, int x, int y, int w, int h, int radius, COLORREF fill, BYTE alpha) {
    int px = Ui(x);
    int py = Ui(y);
    int pw = Ui(w);
    int ph = Ui(h);
    int pr = Ui(radius);
    if (pw <= 0 || ph <= 0) return;

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    Gdiplus::GraphicsPath path;
    AddRoundedRectPath(&path,
                       static_cast<Gdiplus::REAL>(px) + 0.5f,
                       static_cast<Gdiplus::REAL>(py) + 0.5f,
                       static_cast<Gdiplus::REAL>(pw) - 1.0f,
                       static_cast<Gdiplus::REAL>(ph) - 1.0f,
                       static_cast<Gdiplus::REAL>(pr));

    Gdiplus::SolidBrush brush(GdiColor(fill, alpha));
    graphics.FillPath(&brush, &path);
}

COLORREF BlendColor(COLORREF a, COLORREF b, int percentB) {
    percentB = ClampInt(percentB, 0, 100);
    int percentA = 100 - percentB;
    return Rgb((GetRValue(a) * percentA + GetRValue(b) * percentB) / 100,
               (GetGValue(a) * percentA + GetGValue(b) * percentB) / 100,
               (GetBValue(a) * percentA + GetBValue(b) * percentB) / 100);
}

int ColorDistance(COLORREF a, COLORREF b) {
    int dr = GetRValue(a) - GetRValue(b);
    int dg = GetGValue(a) - GetGValue(b);
    int db = GetBValue(a) - GetBValue(b);
    if (dr < 0) dr = -dr;
    if (dg < 0) dg = -dg;
    if (db < 0) db = -db;
    return dr + dg + db;
}

bool IsPrimaryGlassTint(COLORREF color) {
    return ColorDistance(color, g_theme.primary) < 120 ||
           ColorDistance(color, g_theme.primaryHover) < 120 ||
           ColorDistance(color, g_theme.primaryBorderHover) < 150;
}

bool UsesStrongerRevealFeedback(int control) {
    return control == HoverLanguageButton ||
           (control >= HoverLanguageOptionBase && control < HoverSwitchBase) ||
           control == HoverSwitchBase ||
           control == HoverSwitchBase + 1 ||
           control == HoverAutoRestore ||
           control == HoverStartup ||
           control == HoverOk ||
           control == HoverApply ||
           control == HoverCancel ||
           control == HoverSupport ||
           control == HoverSupporterBadge ||
           control == HoverHdrCalibration ||
           control == HoverHdrCalibrationDismiss ||
           IsSettingsTitleControl(control) ||
           control == HoverSupportDonate ||
           control == HoverSupportActivate ||
           control == HoverSupportCode;
}

BYTE AlphaScale(int alpha, int amount) {
    return static_cast<BYTE>(ClampInt(alpha * ClampInt(amount, 0, 1000) / 1000, 0, 255));
}

void TrackedLightPoint(HDC dc, int px, int py, int pw, int ph, int* lightX, int* lightY) {
    *lightX = px + pw / 2;
    *lightY = py + ph / 2;
    if (!g_settingsMouseKnown) return;

    POINT origin = {};
    GetViewportOrgEx(dc, &origin);
    *lightX = ClampInt(g_settingsMousePoint.x - origin.x, px, px + pw);
    *lightY = ClampInt(g_settingsMousePoint.y - origin.y, py, py + ph);
}

void DrawLiquidGlassInteractionGlow(Gdiplus::Graphics* graphics, int pw, int ph,
                                    int lightX, int lightY, int interaction, COLORREF glowColor) {
    if (interaction <= 0 || pw <= 0 || ph <= 0) return;

    int diameter = std::max(pw, ph) * (g_theme.dark ? 5 : 3) / (g_theme.dark ? 4 : 2);
    int gx = lightX - diameter / 2;
    int gy = lightY - diameter / 2;

    Gdiplus::GraphicsPath glowPath;
    glowPath.AddEllipse(static_cast<Gdiplus::REAL>(gx),
                        static_cast<Gdiplus::REAL>(gy),
                        static_cast<Gdiplus::REAL>(diameter),
                        static_cast<Gdiplus::REAL>(diameter));
    Gdiplus::PathGradientBrush glowBrush(&glowPath);
    glowBrush.SetCenterColor(GdiColor(glowColor,
                                      static_cast<BYTE>(ClampInt(interaction / (g_theme.dark ? 3 : 2),
                                                                 0, g_theme.dark ? 42 : 64))));
    Gdiplus::Color surround[] = {GdiColor(glowColor, 0)};
    INT count = 1;
    glowBrush.SetSurroundColors(surround, &count);
    graphics->FillPath(&glowBrush, &glowPath);

}

void DrawLiquidGlassPanel(HDC dc, int x, int y, int w, int h, int radius,
                          COLORREF fill, COLORREF border, int control = HoverNone) {
    int px = Ui(x);
    int py = Ui(y);
    int pw = Ui(w);
    int ph = Ui(h);
    int pr = Ui(radius);
    if (pw <= 0 || ph <= 0) return;

    int interaction = InteractionPercent(control);
    bool pressed = g_pressedControl == control;
    bool strongerReveal = UsesStrongerRevealFeedback(control);
    if (strongerReveal && (g_hoverControl == control || pressed) && interaction < 12) {
        interaction = 12;
    }
    if (pressed && interaction < 36) {
        interaction = 36;
    }
    if (interaction <= 0) {
        DrawRoundedFill(dc, x, y, w, h, radius, fill, border);
        return;
    }
    if (pressed && pw > Ui(4) && ph > Ui(4)) {
        px += Ui(1);
        py += Ui(1);
        pw -= Ui(2);
        ph -= Ui(2);
    }

    bool largeSurface = control == HoverNone && w >= 120 && h >= 60;
    bool tintedSurface = !largeSurface && IsPrimaryGlassTint(fill);
    int brighten = largeSurface ? (g_theme.dark ? 12 : 24) : (g_theme.dark ? 10 : 20);
    if (tintedSurface) {
        brighten += g_theme.dark ? 6 : 8;
    }
    COLORREF top = BlendColor(fill, Rgb(255, 255, 255), brighten + interaction / 16);
    COLORREF middle = BlendColor(fill, Rgb(255, 255, 255), g_theme.dark ? 3 : 2);
    COLORREF bottom = BlendColor(fill, g_theme.dark ? Rgb(0, 0, 0) : Rgb(96, 96, 96),
                                 largeSurface ? (g_theme.dark ? 4 : 4) : (g_theme.dark ? 5 : 5));
    if (pressed) {
        top = BlendColor(top, Rgb(0, 0, 0), g_theme.dark ? 10 : 5);
        middle = BlendColor(middle, Rgb(0, 0, 0), g_theme.dark ? 10 : 5);
        bottom = BlendColor(bottom, Rgb(0, 0, 0), g_theme.dark ? 14 : 7);
    }
    int alphaBase = largeSurface ? (g_theme.dark ? 206 : 232)
                                 : (tintedSurface ? (g_theme.dark ? 154 : 182)
                                                  : (g_theme.dark ? 162 : 188));
    BYTE fillAlpha = static_cast<BYTE>(ClampInt(alphaBase + interaction / 18, 0, 218));

    DrawRoundedAlphaFill(dc, x + 1, y + 3, w - 2, h, radius, Rgb(0, 0, 0),
                         largeSurface ? (g_theme.dark ? 30 : 18) : (g_theme.dark ? 24 : 18));
    DrawRoundedAlphaFill(dc, x, y + 1, w, h, radius, Rgb(0, 0, 0),
                         largeSurface ? (g_theme.dark ? 12 : 8) : (g_theme.dark ? 8 : 8));

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    Gdiplus::GraphicsPath path;
    AddRoundedRectPath(&path,
                       static_cast<Gdiplus::REAL>(px) + 0.5f,
                       static_cast<Gdiplus::REAL>(py) + 0.5f,
                       static_cast<Gdiplus::REAL>(pw) - 1.0f,
                       static_cast<Gdiplus::REAL>(ph) - 1.0f,
                       static_cast<Gdiplus::REAL>(pr));

    Gdiplus::LinearGradientBrush fillBrush(
        Gdiplus::RectF(static_cast<Gdiplus::REAL>(px), static_cast<Gdiplus::REAL>(py),
                       static_cast<Gdiplus::REAL>(pw), static_cast<Gdiplus::REAL>(ph)),
        GdiColor(top, fillAlpha),
        GdiColor(bottom, static_cast<BYTE>(ClampInt(fillAlpha - (g_theme.dark ? 12 : 8), 0, 255))),
        Gdiplus::LinearGradientModeVertical);
    Gdiplus::Color fillColors[] = {
        GdiColor(top, static_cast<BYTE>(ClampInt(fillAlpha + (g_theme.dark ? 4 : 8), 0, 248))),
        GdiColor(middle, fillAlpha),
        GdiColor(bottom, static_cast<BYTE>(ClampInt(fillAlpha - (g_theme.dark ? 14 : 10), 0, 255)))
    };
    Gdiplus::REAL fillPositions[] = {0.0f, 0.52f, 1.0f};
    fillBrush.SetInterpolationColors(fillColors, fillPositions, 3);
    graphics.FillPath(&fillBrush, &path);

    graphics.SetClip(&path);

    int bottomDepthH = Ui(largeSurface ? 18 : 8);
    if (bottomDepthH > 0) {
        COLORREF depth = g_theme.dark ? Rgb(0, 0, 0) : Rgb(43, 72, 96);
        Gdiplus::LinearGradientBrush depthBrush(
            Gdiplus::RectF(static_cast<Gdiplus::REAL>(px),
                           static_cast<Gdiplus::REAL>(py + ph - bottomDepthH),
                           static_cast<Gdiplus::REAL>(pw),
                           static_cast<Gdiplus::REAL>(bottomDepthH)),
            GdiColor(depth, 0),
            GdiColor(depth, static_cast<BYTE>(g_theme.dark ? 10 : 8)),
            Gdiplus::LinearGradientModeVertical);
        graphics.FillRectangle(&depthBrush,
                               static_cast<Gdiplus::REAL>(px),
                               static_cast<Gdiplus::REAL>(py + ph - bottomDepthH),
                               static_cast<Gdiplus::REAL>(pw),
                               static_cast<Gdiplus::REAL>(bottomDepthH));
    }

    if (interaction > 0) {
        int lightX = 0;
        int lightY = 0;
        TrackedLightPoint(dc, px, py, pw, ph, &lightX, &lightY);
        COLORREF glowColor = tintedSurface ? BlendColor(fill, Rgb(255, 255, 255), g_theme.dark ? 55 : 72)
                                           : (g_theme.dark ? Rgb(255, 255, 255)
                                                           : BlendColor(g_theme.primaryHover, Rgb(255, 255, 255), 32));
        int glowInteraction = pressed ? std::min(100, interaction + 18) : interaction;
        if (strongerReveal) {
            glowInteraction = ClampInt(glowInteraction * 2, 0, 100);
        }
        DrawLiquidGlassInteractionGlow(&graphics, pw, ph, lightX, lightY,
                                       glowInteraction,
                                       glowColor);
        COLORREF tint = tintedSurface ? BlendColor(fill, Rgb(255, 255, 255), g_theme.dark ? 45 : 66)
                                       : (g_theme.dark ? Rgb(255, 255, 255)
                                                       : BlendColor(g_theme.primaryHover, Rgb(255, 255, 255), 56));
        int hoverDivisor = strongerReveal ? (tintedSurface ? 24 : (g_theme.dark ? 30 : 12))
                                          : (tintedSurface ? 16 : (g_theme.dark ? 24 : 14));
        int hoverAlpha = interaction / hoverDivisor;
        if (!g_theme.dark && strongerReveal && hoverAlpha < 6) {
            hoverAlpha = 6;
        }
        Gdiplus::SolidBrush hoverBrush(GdiColor(tint, static_cast<BYTE>(ClampInt(hoverAlpha, 0, 24))));
        graphics.FillPath(&hoverBrush, &path);
    }

    graphics.ResetClip();

    Gdiplus::Pen innerPen(Gdiplus::Color(static_cast<BYTE>(largeSurface ? (g_theme.dark ? 82 : 120)
                                                                        : (g_theme.dark ? 42 : 72)),
                                         255, 255, 255),
                          1.0f);
    graphics.DrawPath(&innerPen, &path);

    COLORREF neutralBorder = g_theme.dark ? Rgb(112, 112, 112) : Rgb(178, 186, 196);
    COLORREF borderBase = tintedSurface ? BlendColor(fill, Rgb(255, 255, 255), pressed ? 10 : 6)
                                        : BlendColor(border, neutralBorder, g_theme.dark ? 18 : 46);
    Gdiplus::Pen borderPen(GdiColor(BlendColor(borderBase, Rgb(255, 255, 255), 8 + interaction / 18),
                                    largeSurface ? (g_theme.dark ? 122 : 188) : (g_theme.dark ? 110 : 166)),
                           1.0f);
    graphics.DrawPath(&borderPen, &path);
}

void DrawSettingsCardPanel(HDC dc, int x, int y, int w, int h) {
    DrawRoundedAlphaFill(dc, x + 2, y + 4, w, h, 8, Rgb(0, 0, 0), g_theme.dark ? 38 : 16);
    DrawRoundedFill(dc, x, y, w, h, 8, g_theme.card, g_theme.cardBorder);
}

void DrawSettingsRowHoverBox(HDC dc, int x, int y, int w, int h, int amount, int control = HoverNone) {
    if (control != HoverNone && g_hoverControl == control && amount < 18) {
        amount = 18;
    }
    if (amount <= 0) return;

    COLORREF fill = BlendColor(g_theme.card, g_theme.cardHover, amount);
    DrawRoundedFill(dc, x, y, w, h, 8, fill, fill);

    if (control == HoverNone || g_hoverControl != control) return;

    int px = Ui(x);
    int py = Ui(y);
    int pw = Ui(w);
    int ph = Ui(h);
    int pr = Ui(8);
    if (pw <= 0 || ph <= 0) return;

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    Gdiplus::GraphicsPath path;
    AddRoundedRectPath(&path,
                       static_cast<Gdiplus::REAL>(px) + 0.5f,
                       static_cast<Gdiplus::REAL>(py) + 0.5f,
                       static_cast<Gdiplus::REAL>(pw) - 1.0f,
                       static_cast<Gdiplus::REAL>(ph) - 1.0f,
                       static_cast<Gdiplus::REAL>(pr));
    graphics.SetClip(&path);

    int lightX = 0;
    int lightY = 0;
    TrackedLightPoint(dc, px, py, pw, ph, &lightX, &lightY);
    DrawLiquidGlassInteractionGlow(&graphics, pw, ph, lightX, lightY,
                                   ClampInt(amount * 2, 0, 100),
                                   g_theme.dark ? Rgb(255, 255, 255) : Rgb(64, 178, 255));

    Gdiplus::SolidBrush brush(GdiColor(g_theme.dark ? Rgb(255, 255, 255) : Rgb(64, 178, 255),
                                       static_cast<BYTE>(ClampInt(amount / 6, 0, 18))));
    graphics.FillPath(&brush, &path);
}

void DrawWindowGlassBackdrop(HDC dc, int visibleHeight) {
    (void)dc;
    (void)visibleHeight;
}

void FillRoundedGradient(HDC dc, int x, int y, int w, int h, int radius, COLORREF top, COLORREF bottom) {
    int px = Ui(x);
    int py = Ui(y);
    int pw = Ui(w);
    int ph = Ui(h);
    int pr = Ui(radius);
    if (pw <= 0 || ph <= 0) return;

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    Gdiplus::GraphicsPath path;
    AddRoundedRectPath(&path,
                       static_cast<Gdiplus::REAL>(px) + 0.5f,
                       static_cast<Gdiplus::REAL>(py) + 0.5f,
                       static_cast<Gdiplus::REAL>(pw) - 1.0f,
                       static_cast<Gdiplus::REAL>(ph) - 1.0f,
                       static_cast<Gdiplus::REAL>(pr));

    Gdiplus::LinearGradientBrush brush(
        Gdiplus::RectF(static_cast<Gdiplus::REAL>(px), static_cast<Gdiplus::REAL>(py),
                       static_cast<Gdiplus::REAL>(pw), static_cast<Gdiplus::REAL>(ph)),
        GdiColor(top), GdiColor(bottom), Gdiplus::LinearGradientModeVertical);
    graphics.FillPath(&brush, &path);

    Gdiplus::Pen borderPen(GdiColor(g_theme.controlBorder), 1.0f);
    graphics.DrawPath(&borderPen, &path);
}

std::wstring PercentLabel(int value) {
    return F(TxtPercentValue, {IntText(value)});
}

std::wstring HdrSyncText() {
    if (g_lastHdrTargetCount <= 0) {
        return T(TxtNoHdrScreen);
    }

    if (g_lastHdrSuccessCount >= g_lastHdrTargetCount) {
        return F(TxtHdrSyncAll, {IntText(g_lastHdrTargetCount), IntText(g_lastHdrTargetCount)});
    }
    return F(TxtHdrSyncPartial, {IntText(g_lastHdrSuccessCount), IntText(g_lastHdrTargetCount)});
}

std::wstring HdrPillText() {
    if (g_lastHdrTargetCount <= 0) {
        return T(TxtNoHdrScreen);
    }

    return F(TxtHdrPillStatus, {IntText(g_lastHdrSuccessCount), IntText(g_lastHdrTargetCount)});
}

bool GetDisplayedBrightnessState(int* brightness, bool* night) {
    if (g_transitionActive && g_transitionTargetBrightness >= 0) {
        *brightness = g_transitionTargetBrightness;
        *night = g_transitionNight;
        return true;
    }

    if (g_settingsPreviewActive && g_settingsPreviewBrightness >= 0) {
        *brightness = g_settingsPreviewBrightness;
        *night = g_settingsPreviewNight;
        return true;
    }

    if (g_lastAppliedBrightness >= 0) {
        *brightness = g_lastAppliedBrightness;
        *night = g_lastDecisionNight;
        return true;
    }

    return false;
}

std::wstring HeroStatusText() {
    int brightness = -1;
    bool night = false;
    if (!GetDisplayedBrightnessState(&brightness, &night)) {
        return T(TxtSettingsSubtitle);
    }

    return F(TxtHeroStatus, {PercentLabel(brightness), night ? T(TxtNight) : T(TxtDay), HdrSyncText()});
}

void DrawAppMark(HDC dc, int x, int y) {
    FillRoundedGradient(dc, x + 1, y + 2, 38, 36, 8,
                        Rgb(8, 92, 124),
                        Rgb(28, 184, 255));
    DrawRoundedAlphaFill(dc, x + 19, y + 5, 3, 30, 2, Rgb(230, 250, 255), 220);
    DrawCircleFill(dc, x + 27, y + 9, 8, Rgb(255, 244, 142), Rgb(255, 244, 142));
    DrawRoundedAlphaFill(dc, x + 24, y + 29, 11, 2, 1, Rgb(214, 253, 255), 235);
    DrawRoundedAlphaFill(dc, x + 6, y + 29, 9, 2, 1, Rgb(8, 75, 96), 150);
    DrawRoundedOutline(dc, x + 1, y + 2, 38, 36, 8,
                       g_theme.dark ? Rgb(130, 232, 255) : Rgb(14, 116, 144), 255, 1.8f);
}

void DrawStatusPill(HDC dc, int x, int y, int w, const std::wstring& text, COLORREF accent) {
    DrawLiquidGlassPanel(dc, x, y, w, 28, 14, g_theme.elevated, g_theme.elevated);
    DrawCircleFill(dc, x + 12, y + 10, 8, accent, accent);
    RECT rect = UiBox(x + 26, y, w - 34, 28);
    DrawTextLine(dc, text.c_str(), rect, g_smallFont, g_theme.text,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawChevron(HDC dc, int x, int y, bool up, COLORREF color) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(GdiColor(color), 1.8f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);

    int px = Ui(x);
    int py = Ui(y);
    int s = Ui(8);
    if (up) {
        graphics.DrawLine(&pen, px, py + s / 2, px + s / 2, py);
        graphics.DrawLine(&pen, px + s / 2, py, px + s, py + s / 2);
    } else {
        graphics.DrawLine(&pen, px, py, px + s / 2, py + s / 2);
        graphics.DrawLine(&pen, px + s / 2, py + s / 2, px + s, py);
    }
}

void DrawCheckMark(HDC dc, int x, int y, COLORREF color) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(GdiColor(color), 2.0f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);

    int px = Ui(x);
    int py = Ui(y);
    graphics.DrawLine(&pen, px, py + Ui(5), px + Ui(4), py + Ui(9));
    graphics.DrawLine(&pen, px + Ui(4), py + Ui(9), px + Ui(12), py);
}

void DrawCloseIcon(HDC dc, int x, int y, int size, COLORREF color) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(GdiColor(color), 1.7f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);

    int px = Ui(x);
    int py = Ui(y);
    int ps = Ui(size);
    graphics.DrawLine(&pen, px, py, px + ps, py + ps);
    graphics.DrawLine(&pen, px + ps, py, px, py + ps);
}

void DrawMinimizeIcon(HDC dc, int x, int y, int width, COLORREF color) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(GdiColor(color), 1.8f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);

    int px = Ui(x);
    int py = Ui(y);
    int pw = Ui(width);
    graphics.DrawLine(&pen, px, py, px + pw, py);
}

void DrawQuestionIcon(HDC dc, int x, int y, COLORREF color) {
    DrawCircleFill(dc, x, y, 18, g_theme.dark ? Rgb(10, 12, 14) : Rgb(255, 255, 255),
                   BlendColor(color, g_theme.window, 44));
    RECT rect = UiBox(x, y - 1, 18, 18);
    DrawTextLine(dc, L"?", rect, g_smallFont, color,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawGithubIcon(HDC dc, int x, int y, int size, COLORREF color) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    const Gdiplus::REAL px = static_cast<Gdiplus::REAL>(Ui(x));
    const Gdiplus::REAL py = static_cast<Gdiplus::REAL>(Ui(y));
    const Gdiplus::REAL scale = static_cast<Gdiplus::REAL>(Ui(size)) / 16.0f;
    auto p = [&](Gdiplus::REAL vx, Gdiplus::REAL vy) {
        return Gdiplus::PointF(px + vx * scale, py + vy * scale);
    };

    Gdiplus::GraphicsPath path;
    Gdiplus::REAL cx = 8.0f;
    Gdiplus::REAL cy = 0.0f;
    path.StartFigure();
    auto c = [&](Gdiplus::REAL x1, Gdiplus::REAL y1,
                 Gdiplus::REAL x2, Gdiplus::REAL y2,
                 Gdiplus::REAL x3, Gdiplus::REAL y3) {
        path.AddBezier(p(cx, cy), p(x1, y1), p(x2, y2), p(x3, y3));
        cx = x3;
        cy = y3;
    };

    c(3.58f, 0.00f, 0.00f, 3.58f, 0.00f, 8.00f);
    c(0.00f, 11.54f, 2.29f, 14.53f, 5.47f, 15.59f);
    c(5.87f, 15.66f, 6.02f, 15.42f, 6.02f, 15.21f);
    c(6.02f, 15.02f, 6.01f, 14.39f, 6.01f, 13.72f);
    c(4.00f, 14.09f, 3.48f, 13.23f, 3.32f, 12.78f);
    c(3.23f, 12.55f, 2.84f, 11.84f, 2.50f, 11.65f);
    c(2.22f, 11.50f, 1.82f, 11.13f, 2.49f, 11.12f);
    c(3.12f, 11.11f, 3.57f, 11.70f, 3.72f, 11.94f);
    c(4.44f, 13.15f, 5.59f, 12.81f, 6.05f, 12.60f);
    c(6.12f, 12.08f, 6.33f, 11.73f, 6.56f, 11.53f);
    c(4.78f, 11.33f, 2.92f, 10.64f, 2.92f, 7.58f);
    c(2.92f, 6.71f, 3.23f, 5.99f, 3.74f, 5.43f);
    c(3.66f, 5.23f, 3.38f, 4.41f, 3.82f, 3.31f);
    c(3.82f, 3.31f, 4.49f, 3.10f, 6.02f, 4.13f);
    c(6.66f, 3.95f, 7.34f, 3.86f, 8.02f, 3.86f);
    c(8.70f, 3.86f, 9.38f, 3.95f, 10.02f, 4.13f);
    c(11.55f, 3.09f, 12.22f, 3.31f, 12.22f, 3.31f);
    c(12.66f, 4.41f, 12.38f, 5.23f, 12.30f, 5.43f);
    c(12.81f, 5.99f, 13.12f, 6.70f, 13.12f, 7.58f);
    c(13.12f, 10.65f, 11.25f, 11.33f, 9.47f, 11.53f);
    c(9.76f, 11.78f, 10.01f, 12.26f, 10.01f, 13.01f);
    c(10.01f, 14.08f, 10.00f, 14.94f, 10.00f, 15.21f);
    c(10.00f, 15.42f, 10.15f, 15.67f, 10.55f, 15.59f);
    c(13.81f, 14.49f, 16.00f, 11.44f, 16.00f, 8.00f);
    c(16.00f, 3.58f, 12.42f, 0.00f, 8.00f, 0.00f);
    path.CloseFigure();

    Gdiplus::SolidBrush brush(GdiColor(color));
    graphics.FillPath(&brush, &path);
}

void DrawPlusMinusIcon(HDC dc, int x, int y, int size, bool plus, COLORREF color) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(GdiColor(color), 1.9f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);

    int px = Ui(x);
    int py = Ui(y);
    int ps = Ui(size);
    int mid = py + ps / 2;
    graphics.DrawLine(&pen, px, mid, px + ps, mid);
    if (plus) {
        int center = px + ps / 2;
        graphics.DrawLine(&pen, center, py, center, py + ps);
    }
}

const wchar_t* LanguageChoiceText(int language) {
    return LocalizedLanguageName(CurrentUiLanguage(), language);
}

void DrawLanguageSelector(HDC dc, const SettingsLayout& layout) {
    const int x = layout.languageSegmentX;
    const int y = layout.languageSegmentY;
    const int w = layout.languageSegmentW;
    const int h = layout.languageSegmentH;
    int amount = std::max(InteractionPercent(HoverLanguageButton), g_languageDropdownOpen ? 80 : 0);
    DrawLiquidGlassPanel(dc, x, y, w, h, kPillControlRadius,
                         BlendColor(g_theme.control, g_theme.controlHover, amount),
                         BlendColor(g_theme.controlBorder, g_theme.controlBorderHover, amount),
                         HoverLanguageButton);

    int selected = NormalizeLanguageChoice(g_settingsDraft.language);
    std::wstring text = LanguageChoiceText(selected);
    RECT textRect = UiBox(x + 14, y, w - 42, h);
    DrawTextLine(dc, text.c_str(), textRect, g_smallFont, g_theme.text,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    DrawChevron(dc, x + w - 22, y + 13, g_languageDropdownOpen, g_theme.mutedText);
}

void DrawLanguageDropdown(HDC dc, const SettingsLayout& layout) {
    if (!g_languageDropdownOpen) return;

    const int w = LanguageDropdownWidth();
    const int x = layout.languageSegmentX + layout.languageSegmentW - w;
    const int y = LanguageDropdownY(layout);
    const int pad = 6;
    const int optionH = 34;
    const int optionCount = LanguageOptionCount();
    const int h = LanguageDropdownHeight();
    int openAmount = std::max(180, g_settingsDropdownAnim);
    DrawRoundedAlphaFill(dc, x + 2, y + 4, w, h, 8, Rgb(0, 0, 0), AlphaScale(g_theme.dark ? 95 : 34, openAmount));
    DrawLiquidGlassPanel(dc, x, y, w, h, 8, g_theme.elevated, g_theme.elevated);

    const LanguageOption* options = LanguageOptions();
    int selected = NormalizeLanguageChoice(g_settingsDraft.language);

    for (int i = 0; i < optionCount; ++i) {
        int rowY = y + pad + i * optionH;
        bool hovered = IsHover(HoverLanguageOptionBase + i);
        bool checked = selected == options[i].id;
        int amount = std::max(std::max(InteractionPercent(HoverLanguageOptionBase + i), checked ? 70 : 0),
                              hovered ? 1 : 0);
        if (amount > 0) {
            COLORREF rowFill = BlendColor(g_theme.control, g_theme.controlHover, amount);
            DrawLiquidGlassPanel(dc, x + 6, rowY + 2, w - 12, optionH - 4, 8,
                                 rowFill, rowFill, HoverLanguageOptionBase + i);
        }
        if (checked) {
            DrawRoundedFill(dc, x + 10, rowY + 9, 3, 16, 2, g_theme.primary, g_theme.primary);
            DrawCheckMark(dc, x + w - 28, rowY + 11, g_theme.primary);
        }

        RECT textRect = UiBox(x + 22, rowY, w - 54, optionH);
        DrawTextLine(dc, LocalizedLanguageName(CurrentUiLanguage(), options[i].id),
                     textRect, g_uiFont, checked ? g_theme.titleText : g_theme.text,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

void DrawSettingsScrollbar(HDC dc, const SettingsLayout& layout, int visibleHeight) {
    int scrollViewportHeight = std::max(1, visibleHeight - kSettingsFooterAreaHeight);
    if (layout.footerY <= scrollViewportHeight) return;

    const int trackX = kSettingsClientWidth - 7;
    const int trackY = 14;
    const int trackH = std::max(48, scrollViewportHeight - 20);
    const int thumbH = std::max(36, MulDiv(trackH, scrollViewportHeight, layout.footerY));
    const int maxScroll = std::max(1, layout.footerY - scrollViewportHeight);
    const int thumbTravel = std::max(0, trackH - thumbH);
    const int thumbY = trackY + MulDiv(thumbTravel, g_settingsScrollY, maxScroll);

    DrawRoundedAlphaFill(dc, trackX, trackY, 3, trackH, 2,
                         g_theme.dark ? Rgb(255, 255, 255) : Rgb(0, 0, 0),
                         g_theme.dark ? 10 : 10);
    DrawRoundedAlphaFill(dc, trackX, thumbY, 3, thumbH, 2,
                         g_theme.dark ? Rgb(255, 255, 255) : Rgb(0, 0, 0),
                         g_theme.dark ? 116 : 82);
}

void DrawSegmentedSwitchMode(HDC dc, const SettingsLayout& layout, bool canFollow) {
    const int x = layout.switchModeX;
    const int y = layout.switchModeY;
    const int w = layout.switchModeW;
    const int h = layout.switchModeH;
    const int segmentW = w / 2;
    bool useSystem = g_settingsDraft.followNightLight && canFollow;
    int selected = useSystem ? 0 : 1;

    DrawLiquidGlassPanel(dc, x, y, w, h, kPillControlRadius, g_theme.control, g_theme.controlBorder);
    int hoverSegment = HoverSegment(HoverSwitchBase, 2);
    if (hoverSegment >= 0 && hoverSegment != selected) {
        DrawLiquidGlassPanel(dc, x + hoverSegment * segmentW + 2, y + 2, segmentW - 4, h - 4, kPillControlRadius - 2,
                             g_theme.controlHover, g_theme.controlBorder,
                             HoverSwitchBase + hoverSegment);
    }
    COLORREF selectedFill = hoverSegment == selected ? g_theme.primaryHover : g_theme.primary;
    int selectedAmount = InteractionPercent(HoverSwitchBase + selected);
    selectedFill = BlendColor(selectedFill, g_theme.primaryBorderHover, selectedAmount / 3);
    DrawLiquidGlassPanel(dc, x + selected * segmentW + 2, y + 2, segmentW - 4, h - 4, kPillControlRadius - 2,
                         selectedFill, selectedFill, HoverSwitchBase + selected);

    COLORREF disabled = g_theme.disabledText;
    RECT followRect = UiBox(x, y, segmentW, h);
    RECT defaultRect = UiBox(x + segmentW, y, segmentW, h);
    DrawTextLine(dc, T(TxtFollowSystem), followRect, g_uiFont,
                 selected == 0 ? Rgb(255, 255, 255) : (canFollow ? g_theme.text : disabled),
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DrawTextLine(dc, T(TxtUseDefaultSchedule), defaultRect, g_uiFont,
                 selected == 1 ? Rgb(255, 255, 255) : g_theme.text,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawValuePill(HDC dc, int x, int y, int value) {
    DrawLiquidGlassPanel(dc, x, y, 56, 30, 8, g_theme.control, g_theme.controlBorder);
    std::wstring text = PercentLabel(value);
    RECT rect = UiBox(x, y, 56, 30);
    DrawTextLine(dc, text.c_str(), rect, g_sectionFont, g_theme.titleText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawHotkeyPill(HDC dc, int x, int y, int w, const std::wstring& text, bool recording, bool hovered) {
    COLORREF fill = recording ? g_theme.primary
                              : hovered ? g_theme.controlHover : g_theme.control;
    COLORREF border = recording ? g_theme.primaryHover : g_theme.controlBorder;
    DrawLiquidGlassPanel(dc, x, y, w, 30, 9, fill, border);
    RECT rect = UiBox(x, y, w, 30);
    COLORREF textColor = recording ? Rgb(255, 255, 255) : g_theme.titleText;
    DrawTextLine(dc, text.c_str(), rect, g_uiFont, textColor,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawSlider(HDC dc, int x, int y, int w, int value, bool hovered, int control) {
    int amount = std::max(InteractionPercent(control), hovered ? 1 : 0);
    if (hovered && amount < 18) {
        amount = 18;
    }
    int trackY = y + 15;
    int trackH = 5 + amount / 50;
    DrawRoundedFill(dc, x, trackY, w, trackH, 3,
                    BlendColor(g_theme.track, g_theme.trackHover, amount),
                    BlendColor(g_theme.track, g_theme.trackHover, amount));
    int fillW = MulDiv(w, ClampInt(value, 0, 100), 100);
    if (fillW > 0) {
        COLORREF fill = BlendColor(g_theme.primary, g_theme.primaryHover, amount);
        DrawRoundedFill(dc, x, trackY, fillW, trackH, 3, fill, fill);
    }

    if (amount > 0) {
        DrawRoundedAlphaFill(dc, x, trackY - 5, std::max(1, fillW), 15, 8,
                             g_theme.primaryHover, static_cast<BYTE>(amount / 3));
    }

    if (hovered && amount > 0) {
        int px = Ui(x);
        int py = Ui(trackY - 7);
        int pw = Ui(w);
        int ph = Ui(19);
        if (pw > 0 && ph > 0) {
            Gdiplus::Graphics graphics(dc);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

            Gdiplus::GraphicsPath clipPath;
            AddRoundedRectPath(&clipPath,
                               static_cast<Gdiplus::REAL>(px) + 0.5f,
                               static_cast<Gdiplus::REAL>(py) + 0.5f,
                               static_cast<Gdiplus::REAL>(pw) - 1.0f,
                               static_cast<Gdiplus::REAL>(ph) - 1.0f,
                               static_cast<Gdiplus::REAL>(Ui(9)));
            graphics.SetClip(&clipPath);

            int lightX = 0;
            int lightY = 0;
            TrackedLightPoint(dc, px, py, pw, ph, &lightX, &lightY);
            DrawLiquidGlassInteractionGlow(&graphics, pw, ph, lightX, lightY,
                                           ClampInt(amount * 2, 0, 100),
                                           BlendColor(g_theme.primaryHover, Rgb(255, 255, 255), 28));
        }
    }

    int knobAmount = std::max(amount, hovered ? 100 : 0);
    int knobSize = 18 + knobAmount / 25;
    int knobX = x + fillW - knobSize / 2;
    DrawCircleFill(dc, knobX, y + 7 - knobAmount / 50, knobSize, g_theme.knob,
                   BlendColor(Rgb(102, 192, 255), g_theme.primaryBorderHover, knobAmount));
}

int PreviewArtworkBrightness() {
    if (g_settingsPreviewActive && g_settingsPreviewBrightness >= 0) {
        return g_settingsPreviewBrightness;
    }

    NightDecision decision = DecideNight();
    return SettingsDraftBrightnessForMode(decision.night);
}

COLORREF SdrArtworkColor(COLORREF color, int brightness) {
    brightness = ClampInt(brightness, 0, 100);
    int scale = 72 + brightness * 48 / 100;
    return Rgb(static_cast<BYTE>(ClampInt(GetRValue(color) * scale / 100, 0, 255)),
               static_cast<BYTE>(ClampInt(GetGValue(color) * scale / 100, 0, 255)),
               static_cast<BYTE>(ClampInt(GetBValue(color) * scale / 100, 0, 255)));
}

void DrawContentPreviewScene(HDC dc, int x, int y, int w, int h, bool hdr, int sdrBrightness) {
    const int radius = 8;
    COLORREF skyTop = hdr ? Rgb(18, 128, 232) : SdrArtworkColor(Rgb(56, 139, 212), sdrBrightness);
    COLORREF skyBottom = hdr ? Rgb(105, 211, 255) : SdrArtworkColor(Rgb(128, 199, 224), sdrBrightness);
    FillRoundedGradient(dc, x, y, w, h, radius, skyTop, skyBottom);

    int px = Ui(x);
    int py = Ui(y);
    int pw = Ui(w);
    int ph = Ui(h);
    int pr = Ui(radius);
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    Gdiplus::GraphicsPath clipPath;
    AddRoundedRectPath(&clipPath,
                       static_cast<Gdiplus::REAL>(px) + 0.5f,
                       static_cast<Gdiplus::REAL>(py) + 0.5f,
                       static_cast<Gdiplus::REAL>(pw) - 1.0f,
                       static_cast<Gdiplus::REAL>(ph) - 1.0f,
                       static_cast<Gdiplus::REAL>(pr));
    graphics.SetClip(&clipPath);

    auto pxAt = [&](float amount) -> Gdiplus::REAL {
        return static_cast<Gdiplus::REAL>(px) + static_cast<Gdiplus::REAL>(pw) * amount;
    };
    auto pyAt = [&](float amount) -> Gdiplus::REAL {
        return static_cast<Gdiplus::REAL>(py) + static_cast<Gdiplus::REAL>(ph) * amount;
    };
    auto wScale = [&](float amount) -> Gdiplus::REAL {
        return static_cast<Gdiplus::REAL>(pw) * amount;
    };
    auto hScale = [&](float amount) -> Gdiplus::REAL {
        return static_cast<Gdiplus::REAL>(ph) * amount;
    };

    COLORREF sun = hdr ? Rgb(255, 250, 191) : SdrArtworkColor(Rgb(244, 214, 128), sdrBrightness);
    if (hdr) {
        Gdiplus::SolidBrush glow(Gdiplus::Color(80, 255, 249, 191));
        Gdiplus::REAL glowSize = std::min(wScale(0.34f), hScale(1.18f));
        graphics.FillEllipse(&glow, pxAt(0.78f) - glowSize / 2.0f, pyAt(0.30f) - glowSize / 2.0f,
                             glowSize, glowSize);
    }
    Gdiplus::SolidBrush sunBrush(GdiColor(sun));
    Gdiplus::REAL sunSize = std::min(wScale(0.12f), hScale(0.44f));
    graphics.FillEllipse(&sunBrush, pxAt(0.78f) - sunSize / 2.0f, pyAt(0.30f) - sunSize / 2.0f,
                         sunSize, sunSize);

    COLORREF rearMountain = hdr ? Rgb(39, 97, 120) : SdrArtworkColor(Rgb(59, 95, 112), sdrBrightness);
    COLORREF frontMountain = hdr ? Rgb(27, 145, 93) : SdrArtworkColor(Rgb(49, 127, 88), sdrBrightness);
    Gdiplus::SolidBrush rearBrush(GdiColor(rearMountain));
    Gdiplus::SolidBrush frontBrush(GdiColor(frontMountain));
    Gdiplus::PointF rear[4] = {
        Gdiplus::PointF(pxAt(-0.04f), pyAt(0.58f)),
        Gdiplus::PointF(pxAt(0.24f), pyAt(0.36f)),
        Gdiplus::PointF(pxAt(0.50f), pyAt(0.58f)),
        Gdiplus::PointF(pxAt(1.04f), pyAt(0.58f))
    };
    graphics.FillPolygon(&rearBrush, rear, 4);

    Gdiplus::PointF front[5] = {
        Gdiplus::PointF(pxAt(-0.04f), pyAt(0.72f)),
        Gdiplus::PointF(pxAt(0.38f), pyAt(0.58f)),
        Gdiplus::PointF(pxAt(0.58f), pyAt(0.72f)),
        Gdiplus::PointF(pxAt(1.04f), pyAt(0.72f)),
        Gdiplus::PointF(pxAt(1.04f), pyAt(1.10f))
    };
    graphics.FillPolygon(&frontBrush, front, 5);

    COLORREF waterTop = hdr ? Rgb(30, 190, 210) : SdrArtworkColor(Rgb(79, 149, 170), sdrBrightness);
    COLORREF waterBottom = hdr ? Rgb(16, 99, 188) : SdrArtworkColor(Rgb(60, 103, 153), sdrBrightness);
    Gdiplus::LinearGradientBrush waterBrush(
        Gdiplus::RectF(pxAt(0.0f), pyAt(0.62f), wScale(1.0f), hScale(0.42f)),
        GdiColor(waterTop), GdiColor(waterBottom), Gdiplus::LinearGradientModeVertical);
    graphics.FillRectangle(&waterBrush, pxAt(0.0f), pyAt(0.62f), wScale(1.0f), hScale(0.42f));

    if (hdr) {
        Gdiplus::Pen highlight(Gdiplus::Color(170, 255, 255, 255), std::max(1.2f, hScale(0.025f)));
        graphics.DrawLine(&highlight, pxAt(0.52f), pyAt(0.72f), pxAt(0.88f), pyAt(0.72f));
        graphics.DrawLine(&highlight, pxAt(0.66f), pyAt(0.84f), pxAt(0.82f), pyAt(0.84f));
    }

    graphics.ResetClip();
}

void DrawSdrHdrComparisonAt(HDC dc, int leftX, int y, int imageW, int imageH, int gap) {
    int value = PreviewArtworkBrightness();
    const int rightX = leftX + imageW + gap;

    DrawContentPreviewScene(dc, leftX, y, imageW, imageH, false, value);
    DrawContentPreviewScene(dc, rightX, y, imageW, imageH, true, value);

    std::wstring sdrLabel = std::wstring(T(TxtSdrContent)) + L"  " + PercentLabel(value);
    RECT sdrRect = UiBox(leftX, y + imageH + 6, imageW, 20);
    DrawTextLine(dc, sdrLabel.c_str(), sdrRect, g_smallFont, g_theme.text,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT hdrRect = UiBox(rightX, y + imageH + 6, imageW, 20);
    DrawTextLine(dc, T(TxtHdrContent), hdrRect, g_smallFont, g_theme.text,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawSdrHdrComparison(HDC dc, const SettingsLayout& layout) {
    int leftX = 0;
    int topY = 0;
    int imageW = 0;
    int imageH = 0;
    int gap = 0;
    BrightnessPreviewLayout(layout, &leftX, &topY, &imageW, &imageH, &gap);
    DrawSdrHdrComparisonAt(dc, leftX, topY, imageW, imageH, gap);
}

void ApplyHdrPreviewRegion(HWND hwnd, int width, int height) {
    (void)width;
    (void)height;
    SetWindowRgn(hwnd, NULL, FALSE);
}

LRESULT CALLBACK HdrPreviewWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps = {};
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        HdrPreviewRender(&g_hdrPreview);
        return 0;
    }
    case WM_SIZE:
        if (hwnd == g_hdrPreviewWindow && HdrPreviewHasSwapChain(g_hdrPreview)) {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            HdrPreviewResize(&g_hdrPreview, width, height);
            ApplyHdrPreviewRegion(hwnd, width, height);
            HdrPreviewRender(&g_hdrPreview);
        }
        return 0;
    case WM_MOUSEWHEEL:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE: {
        HWND parent = GetParent(hwnd);
        if (parent) {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (message != WM_MOUSEWHEEL) {
                MapWindowPoints(hwnd, parent, &pt, 1);
                lParam = MAKELPARAM(pt.x, pt.y);
            }
            SendMessageW(parent, message, wParam, lParam);
        }
        return 0;
    }
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return TRUE;
    case WM_DESTROY:
        if (hwnd == g_hdrPreviewWindow) {
            HdrPreviewReleaseDevice(&g_hdrPreview);
            g_hdrPreviewWindow = NULL;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool CreateHdrPreviewWindow(HWND parent) {
    if (g_hdrPreviewWindow) return true;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HdrPreviewWndProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"HdrSdrBrightnessHdrPreviewWindow";
    RegisterClassExW(&wc);

    g_hdrPreviewWindow = CreateWindowExW(0, wc.lpszClassName, L"", WS_CHILD | WS_CLIPSIBLINGS,
                                         0, 0, 1, 1, parent, NULL, g_instance, NULL);
    g_hdrPreview.hwnd = g_hdrPreviewWindow;
    return g_hdrPreviewWindow != NULL;
}

void DestroyHdrPreviewWindow() {
    HWND hwnd = g_hdrPreviewWindow;
    if (hwnd) {
        DestroyWindow(hwnd);
    } else {
        HdrPreviewReleaseDevice(&g_hdrPreview);
    }
    UnregisterClassW(L"HdrSdrBrightnessHdrPreviewWindow", g_instance);
}

void UpdateHdrPreviewWindow(HWND hwnd) {
    if (!hwnd || !g_hdrPreviewWindow) return;

    SettingsLayout layout = BuildSettingsLayout(SettingsDraftUsesSystemSwitching());
    int leftX = 0;
    int contentY = 0;
    int imageW = 0;
    int imageH = 0;
    int gap = 0;
    BrightnessPreviewLayout(layout, &leftX, &contentY, &imageW, &imageH, &gap);
    const int rightX = leftX + imageW + gap;
    int appearOffset = (1000 - ClampInt(g_settingsWindowAnim, 0, 1000)) / 80;
    int visibleY = contentY - g_settingsScrollY + appearOffset;
    int viewportH = SettingsScrollableViewportHeight(hwnd);
    bool fullyVisible = visibleY >= 0 && visibleY + imageH <= viewportH;
    bool shouldShow = fullyVisible && !g_languageDropdownOpen && !g_settingsInfoDialogOpen &&
                      !g_hotkeyDialogOpen && !IsIconic(hwnd);

    int px = Ui(rightX);
    int py = Ui(kSettingsTitleBarHeight + visibleY);
    int pw = Ui(imageW);
    int ph = Ui(imageH);
    SetWindowPos(g_hdrPreviewWindow, HWND_TOP, px, py, pw, ph,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER | (shouldShow ? 0 : SWP_HIDEWINDOW));
    ApplyHdrPreviewRegion(g_hdrPreviewWindow, pw, ph);

    if (!shouldShow || !HdrPreviewEnsureReady(&g_hdrPreview, g_hdrPreviewWindow, pw, ph, g_theme.card) ||
        !HdrPreviewRender(&g_hdrPreview)) {
        ShowWindow(g_hdrPreviewWindow, SW_HIDE);
        return;
    }

    ShowWindow(g_hdrPreviewWindow, SW_SHOWNA);
}

void DrawToggle(HDC dc, int x, int y, bool checked, bool hovered, int control) {
    int amount = std::max(InteractionPercent(control), hovered ? 1 : 0);
    COLORREF fill = checked ? BlendColor(g_theme.primary, g_theme.primaryHover, amount)
                            : BlendColor(g_theme.control, g_theme.controlHover, amount);
    COLORREF border = checked ? BlendColor(g_theme.primary, g_theme.primaryBorderHover, amount / 2)
                              : BlendColor(fill, g_theme.controlBorderHover, amount / 2);
    DrawLiquidGlassPanel(dc, x, y, 44, 24, 12, fill, border, control);
    int knobX = checked ? x + 22 : x + 2;
    int knobSize = 20 + amount / 50;
    DrawCircleFill(dc, knobX - amount / 100, y + 2 - amount / 100, knobSize,
                   Rgb(255, 255, 255), Rgb(255, 255, 255));
}

void DrawSettingRowText(HDC dc, const wchar_t* text, int x, int y, int w) {
    RECT rect = UiBox(x, y, w, 28);
    DrawTextLine(dc, text, rect, g_uiFont, g_theme.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawCardSeparator(HDC dc, int x, int y, int w) {
    DrawRoundedAlphaFill(dc, x, y, w, 1, 1,
                         Rgb(0, 0, 0),
                         g_theme.dark ? 58 : 14);
}

void DrawTimeStepper(HDC dc, const SettingsLayout& layout, const wchar_t* label, int y, int hour, int minute,
                     bool minusHover, bool plusHover, int minusControl, int plusControl) {
    DrawSettingRowText(dc, label, layout.cardX + kSettingsCardPadding, y, 210);
    int minusAmount = std::max(InteractionPercent(minusControl), minusHover ? 1 : 0);
    int plusAmount = std::max(InteractionPercent(plusControl), plusHover ? 1 : 0);
    DrawLiquidGlassPanel(dc, TimeStepperMinusX(layout), y - 4, 28, 28, 8,
                         BlendColor(g_theme.control, g_theme.controlHover, minusAmount),
                         BlendColor(g_theme.controlBorder, g_theme.controlBorderHover, minusAmount),
                         minusControl);
    DrawLiquidGlassPanel(dc, TimeStepperPlusX(layout), y - 4, 28, 28, 8,
                         BlendColor(g_theme.control, g_theme.controlHover, plusAmount),
                         BlendColor(g_theme.controlBorder, g_theme.controlBorderHover, plusAmount),
                         plusControl);
    DrawLiquidGlassPanel(dc, TimeStepperValueX(layout), y - 6, 88, 32, 8, g_theme.elevated, g_theme.elevated);

    RECT timeRect = UiBox(TimeStepperValueX(layout), y - 6, 88, 32);
    std::wstring time = night_mode::TimeText(hour, minute);
    DrawTextLine(dc, time.c_str(), timeRect, g_sectionFont, g_theme.titleText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawPlusMinusIcon(dc, TimeStepperMinusX(layout) + 9, y + 7, 10, false, g_theme.titleText);
    DrawPlusMinusIcon(dc, TimeStepperPlusX(layout) + 9, y + 7, 10, true, g_theme.titleText);
}

void DrawFooterButton(HDC dc, int x, int y, int w, const wchar_t* text, bool primary, bool hovered, int control) {
    int amount = std::max(InteractionPercent(control), hovered ? 1 : 0);
    bool pressed = g_pressedControl == control || g_supportPressedControl == control;
    COLORREF fill = primary ? BlendColor(g_theme.primary, g_theme.primaryHover, amount)
                            : BlendColor(g_theme.window, g_theme.control, 82 + amount / 8);
    COLORREF border = primary ? BlendColor(g_theme.primary, g_theme.primaryBorderHover, amount)
                              : BlendColor(fill, g_theme.controlBorderHover, amount / 2);
    if (pressed) {
        fill = BlendColor(fill, Rgb(0, 0, 0), g_theme.dark ? 10 : 4);
    }
    COLORREF color = primary ? Rgb(255, 255, 255) : g_theme.text;
    DrawLiquidGlassPanel(dc, x, y, w, 34, 17, fill, border, control);
    RECT rect = UiBox(x + 8, y + (pressed ? 1 : 0), w - 16, 34);
    DrawTextLine(dc, text, rect, g_sectionFont, color,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawDialogButton(HDC dc, int x, int y, int w, const wchar_t* text, int control) {
    bool hovered = IsHover(control);
    bool pressed = g_pressedControl == control;
    int amount = std::max(InteractionPercent(control), hovered ? 1 : 0);
    COLORREF fill = pressed ? g_theme.primaryBorderHover : BlendColor(g_theme.primary, g_theme.primaryHover, amount);
    DrawLiquidGlassPanel(dc, x, y, w, 34, 17, fill, fill, control);
    RECT rect = UiBox(x, y + (pressed ? 1 : 0), w, 34);
    DrawTextLine(dc, text, rect, g_sectionFont, Rgb(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawWindowsLogoIcon(HDC dc, int x, int y) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    const int tile = 8;
    const int gap = 2;
    Gdiplus::SolidBrush red(GdiColor(Rgb(242, 80, 34)));
    Gdiplus::SolidBrush green(GdiColor(Rgb(127, 186, 0)));
    Gdiplus::SolidBrush blue(GdiColor(Rgb(0, 164, 239)));
    Gdiplus::SolidBrush yellow(GdiColor(Rgb(255, 185, 0)));
    graphics.FillRectangle(&red, Ui(x), Ui(y), Ui(tile), Ui(tile));
    graphics.FillRectangle(&green, Ui(x + tile + gap), Ui(y), Ui(tile), Ui(tile));
    graphics.FillRectangle(&blue, Ui(x), Ui(y + tile + gap), Ui(tile), Ui(tile));
    graphics.FillRectangle(&yellow, Ui(x + tile + gap), Ui(y + tile + gap), Ui(tile), Ui(tile));
}

void DrawPixelCapsuleBubble(HDC dc, int x, int y, int w, int h,
                            COLORREF fill, COLORREF border, COLORREF highlight, COLORREF shadow,
                            int tailCenterX) {
    DrawSolidLogicalRect(dc, x + 2, y + 3, w, h - 1, shadow);

    DrawSolidLogicalRect(dc, x + 7, y, w - 14, 1, highlight);
    DrawSolidLogicalRect(dc, x + 4, y + 1, w - 8, 1, border);
    DrawSolidLogicalRect(dc, x + 2, y + 2, w - 4, 2, border);
    DrawSolidLogicalRect(dc, x, y + 6, 2, h - 12, border);
    DrawSolidLogicalRect(dc, x + w - 2, y + 6, 2, h - 12, border);
    DrawSolidLogicalRect(dc, x + 2, y + h - 4, w - 4, 2, border);
    DrawSolidLogicalRect(dc, x + 4, y + h - 2, w - 8, 1, border);

    DrawSolidLogicalRect(dc, x + 4, y + 4, w - 8, h - 8, fill);
    DrawSolidLogicalRect(dc, x + 2, y + 6, w - 4, h - 12, fill);
    DrawSolidLogicalRect(dc, x + 8, y + 3, w - 16, 1, highlight);
    DrawSolidLogicalRect(dc, x + 3, y + 7, 1, h - 14, highlight);
    DrawSolidLogicalRect(dc, x + 8, y + h - 6, w - 16, 1, BlendColor(fill, border, 28));
    DrawSolidLogicalRect(dc, x + w - 5, y + 7, 1, h - 14, BlendColor(fill, border, 24));
    DrawSolidLogicalRect(dc, x + 5, y + 5, 2, 2, highlight);
    DrawSolidLogicalRect(dc, x + w - 8, y + h - 7, 2, 2, border);

    const int tailY = y + h - 1;
    DrawSolidLogicalRect(dc, tailCenterX - 5, tailY - 1, 10, 2, border);
    DrawSolidLogicalRect(dc, tailCenterX - 3, tailY + 1, 6, 2, border);
    DrawSolidLogicalRect(dc, tailCenterX - 1, tailY + 3, 2, 2, border);
    DrawSolidLogicalRect(dc, tailCenterX - 3, tailY - 1, 6, 2, fill);
    DrawSolidLogicalRect(dc, tailCenterX - 1, tailY + 1, 2, 2, fill);
}

void DrawPixelStoreBadge(HDC dc, int x, int y, bool pressed, COLORREF fill, COLORREF border) {
    const int offset = pressed ? 1 : 0;
    DrawSolidLogicalRect(dc, x + 2, y + 3, 24, 24, Rgb(0, 0, 0));
    DrawSolidLogicalRect(dc, x + offset + 2, y + offset, 20, 1, border);
    DrawSolidLogicalRect(dc, x + offset + 1, y + offset + 1, 22, 1, border);
    DrawSolidLogicalRect(dc, x + offset, y + offset + 3, 1, 18, border);
    DrawSolidLogicalRect(dc, x + offset + 23, y + offset + 3, 1, 18, border);
    DrawSolidLogicalRect(dc, x + offset + 1, y + offset + 22, 22, 1, border);
    DrawSolidLogicalRect(dc, x + offset + 2, y + offset + 2, 20, 20, fill);
    DrawWindowsLogoIcon(dc, x + offset + 3, y + offset + 3);
}

void DrawPixelBubbleEmoji(HDC dc, int x, int y, int mode) {
    COLORREF outline = Rgb(2, 6, 10);
    if (mode == 1) {
        COLORREF yellow = Rgb(250, 204, 21);
        COLORREF orange = Rgb(245, 158, 11);
        DrawSolidLogicalRect(dc, x + 6, y, 4, 5, outline);
        DrawSolidLogicalRect(dc, x + 4, y + 4, 7, 4, outline);
        DrawSolidLogicalRect(dc, x + 2, y + 8, 7, 3, outline);
        DrawSolidLogicalRect(dc, x + 5, y + 10, 4, 5, outline);
        DrawSolidLogicalRect(dc, x + 7, y + 1, 2, 4, yellow);
        DrawSolidLogicalRect(dc, x + 5, y + 5, 5, 2, yellow);
        DrawSolidLogicalRect(dc, x + 3, y + 8, 5, 2, orange);
        DrawSolidLogicalRect(dc, x + 6, y + 10, 2, 4, orange);
        DrawSolidLogicalRect(dc, x + 10, y + 7, 3, 2, yellow);
        DrawSolidLogicalRect(dc, x + 1, y + 1, 2, 2, Rgb(255, 244, 179));
        return;
    }

    if (mode == 2) {
        COLORREF red = Rgb(239, 68, 68);
        COLORREF deep = Rgb(185, 28, 28);
        COLORREF shine = Rgb(254, 202, 202);
        DrawSolidLogicalRect(dc, x + 2, y + 1, 4, 3, outline);
        DrawSolidLogicalRect(dc, x + 9, y + 1, 4, 3, outline);
        DrawSolidLogicalRect(dc, x + 1, y + 4, 13, 4, outline);
        DrawSolidLogicalRect(dc, x + 2, y + 8, 11, 3, outline);
        DrawSolidLogicalRect(dc, x + 4, y + 11, 7, 3, outline);
        DrawSolidLogicalRect(dc, x + 6, y + 14, 3, 1, outline);
        DrawSolidLogicalRect(dc, x + 3, y + 2, 2, 2, red);
        DrawSolidLogicalRect(dc, x + 10, y + 2, 2, 2, red);
        DrawSolidLogicalRect(dc, x + 2, y + 5, 11, 2, red);
        DrawSolidLogicalRect(dc, x + 3, y + 8, 9, 2, red);
        DrawSolidLogicalRect(dc, x + 5, y + 11, 5, 2, deep);
        DrawSolidLogicalRect(dc, x + 7, y + 13, 1, 1, deep);
        DrawSolidLogicalRect(dc, x + 3, y + 4, 2, 1, shine);
        return;
    }

    COLORREF gem = Rgb(45, 212, 191);
    COLORREF gemDark = Rgb(13, 148, 136);
    COLORREF gemLight = Rgb(153, 246, 228);
    DrawSolidLogicalRect(dc, x + 5, y, 5, 2, outline);
    DrawSolidLogicalRect(dc, x + 3, y + 2, 9, 2, outline);
    DrawSolidLogicalRect(dc, x + 2, y + 4, 11, 6, outline);
    DrawSolidLogicalRect(dc, x + 4, y + 10, 7, 3, outline);
    DrawSolidLogicalRect(dc, x + 6, y + 13, 3, 2, outline);
    DrawSolidLogicalRect(dc, x + 6, y + 1, 3, 1, gemLight);
    DrawSolidLogicalRect(dc, x + 4, y + 3, 7, 1, gemLight);
    DrawSolidLogicalRect(dc, x + 3, y + 5, 9, 4, gem);
    DrawSolidLogicalRect(dc, x + 5, y + 9, 5, 2, gem);
    DrawSolidLogicalRect(dc, x + 7, y + 11, 1, 2, gemDark);
    DrawSolidLogicalRect(dc, x + 9, y + 5, 2, 4, gemDark);
}

void DrawSupportButton(HDC dc, int x, int y) {
    int amount = std::max(InteractionPercent(HoverSupport), IsHover(HoverSupport) ? 1 : 0);
    bool pressed = g_pressedControl == HoverSupport;
    UNREFERENCED_PARAMETER(dc);
    const int bubbleW = 168;
    const int bubbleH = 24;
    const int iconX = x + bubbleW - 30;
    const int iconY = y + bubbleH + 4;
    const int pressOffset = pressed ? 1 : 0;
    COLORREF bubbleFill = g_theme.dark ? Rgb(20, 48, 35) : Rgb(252, 246, 219);
    COLORREF bubbleBorder = g_theme.dark ? Rgb(218, 179, 84) : Rgb(122, 83, 34);
    COLORREF bubbleHighlight = g_theme.dark ? Rgb(255, 236, 166) : Rgb(255, 249, 220);
    COLORREF bubbleShadow = g_theme.dark ? Rgb(4, 12, 9) : Rgb(166, 130, 75);
    COLORREF text = g_theme.dark ? Rgb(255, 244, 204) : Rgb(54, 38, 18);
    COLORREF iconFill = g_theme.dark ? Rgb(18, 38, 30) : Rgb(255, 250, 230);
    COLORREF iconBorder = BlendColor(bubbleBorder, g_theme.primary, amount / 4);

    DrawPixelCapsuleBubble(dc, x, y + pressOffset, bubbleW, bubbleH,
                           bubbleFill, bubbleBorder, bubbleHighlight, bubbleShadow,
                           iconX + 12);

    DrawPixelBubbleEmoji(dc, x + 8, y + pressOffset + 5, StoreBubbleMode());

    RECT rect = UiBox(x + 30, y + pressOffset, bubbleW - 42, bubbleH);
    DrawTextLine(dc, T(StoreBubbleTextId()), rect, g_smallFont, text,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    DrawPixelStoreBadge(dc, iconX, iconY, pressed, iconFill, iconBorder);
}

int TextWidthLogical(HDC dc, const wchar_t* text, HFONT font) {
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE size = {};
    GetTextExtentPoint32W(dc, text, static_cast<int>(std::wcslen(text)), &size);
    SelectObject(dc, oldFont);
    return FromUi(size.cx);
}

void DrawUnsavedBadge(HDC dc, int x, int y) {
    const int w = ClampInt(TextWidthLogical(dc, T(TxtUnsavedChanges), g_smallFont) + 52, 118, 190);
    DrawRoundedFill(dc, x, y, w, 26, 13,
                    g_theme.dark ? Rgb(74, 48, 13) : Rgb(255, 248, 220),
                    g_theme.dark ? Rgb(151, 99, 18) : Rgb(220, 170, 74));
    DrawCircleFill(dc, x + 12, y + 9, 8, Rgb(245, 158, 11), Rgb(245, 158, 11));
    RECT rect = UiBox(x + 28, y, w - 36, 26);
    DrawTextLine(dc, T(TxtUnsavedChanges), rect, g_smallFont,
                 g_theme.dark ? Rgb(253, 230, 138) : Rgb(116, 72, 8),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawMedalIcon(HDC dc, int x, int y) {
    {
        Gdiplus::Graphics graphics(dc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        Gdiplus::Point leftRibbon[4] = {
            Gdiplus::Point(Ui(x + 5), Ui(y + 14)),
            Gdiplus::Point(Ui(x + 12), Ui(y + 14)),
            Gdiplus::Point(Ui(x + 10), Ui(y + 25)),
            Gdiplus::Point(Ui(x + 4), Ui(y + 22))
        };
        Gdiplus::Point rightRibbon[4] = {
            Gdiplus::Point(Ui(x + 12), Ui(y + 14)),
            Gdiplus::Point(Ui(x + 19), Ui(y + 14)),
            Gdiplus::Point(Ui(x + 20), Ui(y + 22)),
            Gdiplus::Point(Ui(x + 14), Ui(y + 25))
        };
        Gdiplus::SolidBrush leftBrush(GdiColor(Rgb(37, 99, 235)));
        Gdiplus::SolidBrush rightBrush(GdiColor(g_theme.dark ? Rgb(220, 38, 38) : Rgb(239, 68, 68)));
        graphics.FillPolygon(&leftBrush, leftRibbon, 4);
        graphics.FillPolygon(&rightBrush, rightRibbon, 4);
    }

    COLORREF medal = g_theme.dark ? Rgb(250, 204, 21) : Rgb(245, 158, 11);
    COLORREF medalBorder = g_theme.dark ? Rgb(253, 224, 71) : Rgb(180, 83, 9);
    DrawCircleFill(dc, x + 3, y + 1, 18, medal, medalBorder);
    DrawCircleFill(dc, x + 7, y + 5, 10, g_theme.dark ? Rgb(146, 64, 14) : Rgb(255, 251, 235), medalBorder);
    RECT star = UiBox(x + 6, y + 3, 12, 12);
    DrawTextLine(dc, L"★", star, g_smallFont, medalBorder,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawSupporterBadge(HDC dc, int x, int y) {
    const int w = SupporterBadgeWidth(dc);
    int amount = std::max(InteractionPercent(HoverSupporterBadge), IsHover(HoverSupporterBadge) ? 1 : 0);
    COLORREF fill = BlendColor(g_theme.dark ? Rgb(49, 42, 24) : Rgb(255, 251, 235),
                               g_theme.dark ? Rgb(71, 57, 28) : Rgb(254, 243, 199),
                               amount / 2);
    COLORREF border = BlendColor(g_theme.dark ? Rgb(180, 83, 9) : Rgb(217, 119, 6),
                                 g_theme.dark ? Rgb(253, 224, 71) : Rgb(245, 158, 11),
                                 35 + amount / 3);
    DrawLiquidGlassPanel(dc, x, y, w, 30, 15, fill, border, HoverSupporterBadge);
    DrawMedalIcon(dc, x + 10, y + 2);
    RECT rect = UiBox(x + 42, y + 1, w - 50, 28);
    DrawTextLine(dc, T(TxtSupporterBadge), rect, g_smallFont,
                 g_theme.dark ? Rgb(254, 240, 138) : Rgb(120, 53, 15),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawSupporterTooltip(HDC dc, int x, int y) {
    const wchar_t* text = T(TxtSupporterTooltip);
    int textW = TextWidthLogical(dc, text, g_smallFont);
    int w = ClampInt(textW + 28, 130, 220);
    int h = 32;
    DrawRoundedFill(dc, x, y, w, h, 8,
                    g_theme.dark ? Rgb(30, 32, 34) : Rgb(255, 255, 255),
                    g_theme.dark ? Rgb(76, 79, 84) : Rgb(209, 213, 219));
    RECT rect = UiBox(x + 14, y, w - 28, h);
    DrawTextLine(dc, text, rect, g_smallFont, g_theme.titleText,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawSettingsTitleButton(HDC dc, int control) {
    const int x = SettingsTitleButtonX(control);
    const int y = 5;
    int amount = std::max(InteractionPercent(control), IsHover(control) ? 1 : 0);
    bool pressed = g_pressedControl == control;
    COLORREF icon = g_theme.mutedText;

    if (control == HoverTitleClose && (amount > 0 || pressed)) {
        COLORREF fill = pressed ? Rgb(196, 43, 28) : Rgb(232, 17, 35);
        DrawRoundedAlphaFill(dc, x, y, 38, 34, 8, fill, 232);
        icon = Rgb(255, 255, 255);
    } else if (amount > 0 || pressed) {
        COLORREF fill = pressed ? BlendColor(g_theme.controlHover, Rgb(0, 0, 0), g_theme.dark ? 10 : 4)
                                : BlendColor(g_theme.control, g_theme.controlHover, 72);
        DrawLiquidGlassPanel(dc, x, y, 38, 34, 8, fill, fill, control);
        icon = g_theme.titleText;
    }

    int offset = pressed ? 1 : 0;
    switch (control) {
    case HoverTitleHelp:
        DrawQuestionIcon(dc, x + 10, y + 8 + offset, icon);
        break;
    case HoverTitleGithub:
        DrawGithubIcon(dc, x + 10, y + 8 + offset, 18, icon);
        break;
    case HoverTitleMinimize:
        DrawMinimizeIcon(dc, x + 14, y + 17 + offset, 10, icon);
        break;
    case HoverTitleClose:
        DrawCloseIcon(dc, x + 14, y + 12 + offset, 10, icon);
        break;
    }
}

const wchar_t* SettingsTitleTooltipText(int control) {
    int language = CurrentUiLanguage();
    bool simplified = language == LangChinese;
    bool traditional = language == LangChineseTraditional;

    switch (control) {
    case HoverTitleHelp:
        if (simplified) return L"查看帮助";
        if (traditional) return L"查看說明";
        return L"View help";
    case HoverTitleGithub:
        if (simplified || traditional) return L"访问 GitHub";
        return L"Visit GitHub";
    case HoverTitleMinimize:
        if (simplified) return L"最小化";
        if (traditional) return L"最小化";
        return L"Minimize";
    case HoverTitleClose:
        if (simplified) return L"关闭";
        if (traditional) return L"關閉";
        return L"Close";
    default:
        return L"";
    }
}

void DrawSettingsTitleTooltip(HDC dc) {
    if (!IsSettingsTitleControl(g_hoverControl) || g_pressedControl == g_hoverControl) return;

    const wchar_t* text = SettingsTitleTooltipText(g_hoverControl);
    if (!text || !text[0]) return;

    int textW = TextWidthLogical(dc, text, g_smallFont);
    int w = ClampInt(textW + 24, 70, 136);
    int h = 28;
    int buttonX = SettingsTitleButtonX(g_hoverControl);
    int x = ClampInt(buttonX + 19 - w / 2, 8, kSettingsClientWidth - w - 8);
    int y = kSettingsTitleBarHeight + 6;

    DrawRoundedAlphaFill(dc, x + 2, y + 3, w, h, 7, Rgb(0, 0, 0), g_theme.dark ? 92 : 36);
    DrawRoundedFill(dc, x, y, w, h, 7,
                    g_theme.dark ? Rgb(34, 37, 40) : Rgb(255, 255, 255),
                    g_theme.dark ? Rgb(84, 88, 94) : Rgb(203, 213, 225));
    RECT rect = UiBox(x + 12, y, w - 24, h);
    DrawTextLine(dc, text, rect, g_smallFont, g_theme.titleText,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawSettingsTitleBar(HWND hwnd, HDC dc) {
    (void)hwnd;
    RECT titleBar = UiBox(0, 0, kSettingsClientWidth, kSettingsTitleBarHeight);
    FillRect(dc, &titleBar, g_windowBrush);
    DrawRoundedAlphaFill(dc, 0, kSettingsTitleBarHeight - 1, kSettingsClientWidth, 1, 1,
                         g_theme.dark ? Rgb(255, 255, 255) : Rgb(0, 0, 0),
                         g_theme.dark ? 18 : 16);

    RECT title = UiBox(16, 0, 430, kSettingsTitleBarHeight);
    DrawTextLine(dc, T(TxtSettingsTitle), title, g_smallFont, g_theme.mutedText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    DrawSettingsTitleButton(dc, HoverTitleHelp);
    DrawSettingsTitleButton(dc, HoverTitleGithub);
    DrawSettingsTitleButton(dc, HoverTitleMinimize);
    DrawSettingsTitleButton(dc, HoverTitleClose);
}

void DrawSettingsFooter(HDC dc, int visibleHeight) {
    int footerTop = visibleHeight - kSettingsFooterAreaHeight;
    RECT footerRect = UiBox(0, footerTop, kSettingsClientWidth, kSettingsFooterAreaHeight);
    FillRect(dc, &footerRect, g_windowBrush);

    int footerY = footerTop + 19;
    const int buttonW = 84;
    const int buttonGap = 6;
    const int buttonRight = 28 + 584 - kSettingsCardPadding;
    const int cancelX = buttonRight - buttonW;
    const int applyX = cancelX - buttonGap - buttonW;
    const int okX = applyX - buttonGap - buttonW;

    std::wstring version = AppVersionLabel();
    RECT versionRect = UiBox(28 + kSettingsCardPadding, footerY, 70, 34);
    DrawTextLine(dc, version.c_str(), versionRect, g_smallFont, g_theme.mutedText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    bool unsaved = SettingsDraftHasUnsavedChanges();
    if (unsaved) {
        DrawUnsavedBadge(dc, 132, footerY + 4);
    }

    DrawFooterButton(dc, okX, footerY, buttonW, T(TxtOk), true, IsHover(HoverOk), HoverOk);
    DrawFooterButton(dc, applyX, footerY, buttonW, T(TxtApply), false, IsHover(HoverApply), HoverApply);
    DrawFooterButton(dc, cancelX, footerY, buttonW, T(TxtCancel), false, IsHover(HoverCancel), HoverCancel);
}

void DrawTextBlock(HDC dc, const std::wstring& text, RECT rect, HFONT font, COLORREF color, UINT format) {
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), -1, &rect, format);
    SelectObject(dc, oldFont);
}

void DrawInfoIcon(HDC dc, int x, int y) {
    DrawCircleFill(dc, x, y, 34, g_theme.primary, g_theme.primaryBorderHover);
    RECT rect = UiBox(x, y - 1, 34, 34);
    DrawTextLine(dc, L"i", rect, g_heroFont, Rgb(255, 255, 255),
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawSettingsInfoDialog(HDC dc, HWND hwnd) {
    if (!g_settingsInfoDialogOpen) return;

    int dialogAmount = std::max(260, g_settingsDialogAnim);
    DrawRoundedAlphaFill(dc, 0, 0, kSettingsClientWidth, SettingsContentClientHeight(hwnd), 0,
                         Rgb(0, 0, 0), AlphaScale(g_theme.dark ? 118 : 72, dialogAmount));

    RECT box = SettingsDialogBox(hwnd);
    int x = FromUi(box.left);
    int y = FromUi(box.top);
    int w = FromUi(box.right - box.left);
    int h = FromUi(box.bottom - box.top);

    DrawRoundedAlphaFill(dc, x + 3, y + 5, w, h, 8, Rgb(0, 0, 0), AlphaScale(g_theme.dark ? 130 : 34, dialogAmount));
    DrawLiquidGlassPanel(dc, x, y, w, h, 8, g_theme.elevated, g_theme.controlBorder);

    RECT titleRect = UiBox(x + 24, y + 18, w - 70, 26);
    DrawTextLine(dc, T(TxtDisplayName), titleRect, g_sectionFont, g_theme.titleText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    bool closeHover = IsHover(HoverDialogClose);
    bool closePressed = g_pressedControl == HoverDialogClose;
    if (closeHover || closePressed) {
        DrawLiquidGlassPanel(dc, x + w - 46, y + 12, 32, 32, 8,
                             g_theme.controlHover, g_theme.controlHover, HoverDialogClose);
    }
    DrawCloseIcon(dc, x + w - 35, y + 23 + (closePressed ? 1 : 0), 10, g_theme.text);

    DrawInfoIcon(dc, x + 26, y + 68);
    RECT bodyRect = UiBox(x + 74, y + 62, w - 104, 54);
    DrawTextBlock(dc, T(TxtNightLightUnavailable), bodyRect, g_uiFont, g_theme.text,
                  DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

    bool linkHover = IsHover(HoverDialogLink);
    RECT linkRect = UiBox(x + 74, y + 126, w - 104, 24);
    COLORREF linkColor = linkHover ? g_theme.primaryHover : g_theme.primary;
    DrawTextLine(dc, T(TxtMenuNightLightSettings), linkRect, g_uiFont, linkColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    int underlineWidth = std::min(TextWidthLogical(dc, T(TxtMenuNightLightSettings), g_uiFont), w - 104);
    DrawRoundedFill(dc, x + 74, y + 146, underlineWidth, 1, 1, linkColor, linkColor);

    DrawRoundedAlphaFill(dc, x, y + h - 64, w, 1, 1,
                         g_theme.dark ? Rgb(255, 255, 255) : Rgb(0, 0, 0),
                         g_theme.dark ? 24 : 16);
    DrawDialogButton(dc, x + w - 102, y + h - 54, 78, T(TxtOk), HoverDialogOk);
}

void DrawHotkeyDialog(HDC dc, HWND hwnd) {
    if (!g_hotkeyDialogOpen) return;

    int dialogAmount = std::max(260, g_settingsDialogAnim);
    DrawRoundedAlphaFill(dc, 0, 0, kSettingsClientWidth, SettingsContentClientHeight(hwnd), 0,
                         Rgb(0, 0, 0), AlphaScale(g_theme.dark ? 118 : 72, dialogAmount));

    RECT box = SettingsDialogBox(hwnd);
    int x = FromUi(box.left);
    int y = FromUi(box.top);
    int w = FromUi(box.right - box.left);
    int h = FromUi(box.bottom - box.top);

    DrawRoundedAlphaFill(dc, x + 3, y + 5, w, h, 8, Rgb(0, 0, 0), AlphaScale(g_theme.dark ? 130 : 34, dialogAmount));
    DrawLiquidGlassPanel(dc, x, y, w, h, 8, g_theme.elevated, g_theme.controlBorder);

    RECT titleRect = UiBox(x + 24, y + 18, w - 70, 26);
    DrawTextLine(dc, T(TxtScreenshotHotkeys), titleRect, g_sectionFont, g_theme.titleText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    bool closeHover = IsHover(HoverDialogClose);
    bool closePressed = g_pressedControl == HoverDialogClose;
    if (closeHover || closePressed) {
        DrawLiquidGlassPanel(dc, x + w - 46, y + 12, 32, 32, 8,
                             g_theme.controlHover, g_theme.controlHover, HoverDialogClose);
    }
    DrawCloseIcon(dc, x + w - 35, y + 23 + (closePressed ? 1 : 0), 10, g_theme.text);

    RECT row1 = UiBox(x + 28, y + 72, w - 56, 30);
    RECT row2 = UiBox(x + 28, y + 118, w - 56, 30);
    DrawSettingsRowHoverBox(dc, x + 20, y + 67, w - 40, 40, InteractionPercent(HoverScreenshotHotkey));
    DrawSettingsRowHoverBox(dc, x + 20, y + 113, w - 40, 40, InteractionPercent(HoverFullscreenHotkey));
    DrawTextLine(dc, T(TxtHotkeyScreenshot), row1, g_uiFont, g_theme.text,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DrawTextLine(dc, T(TxtHotkeyFullscreen), row2, g_uiFont, g_theme.text,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    bool recordingScreenshot = g_recordingHotkey == HoverScreenshotHotkey;
    bool recordingFullscreen = g_recordingHotkey == HoverFullscreenHotkey;
    std::wstring screenshotText = recordingScreenshot ? T(TxtHotkeyRecording)
        : HotkeyText(g_settingsDraft.screenshotHotkeyMod, g_settingsDraft.screenshotHotkeyVk);
    std::wstring fullscreenText = recordingFullscreen ? T(TxtHotkeyRecording)
        : HotkeyText(g_settingsDraft.fullscreenHotkeyMod, g_settingsDraft.fullscreenHotkeyVk);
    DrawHotkeyPill(dc, FromUi(HotkeyDialogScreenshotBox(hwnd).left), FromUi(HotkeyDialogScreenshotBox(hwnd).top), 150,
                   screenshotText, recordingScreenshot, IsHover(HoverScreenshotHotkey));
    DrawHotkeyPill(dc, FromUi(HotkeyDialogFullscreenBox(hwnd).left), FromUi(HotkeyDialogFullscreenBox(hwnd).top), 150,
                   fullscreenText, recordingFullscreen, IsHover(HoverFullscreenHotkey));

    DrawRoundedAlphaFill(dc, x, y + h - 64, w, 1, 1,
                         g_theme.dark ? Rgb(255, 255, 255) : Rgb(0, 0, 0),
                         g_theme.dark ? 24 : 16);
    DrawDialogButton(dc, x + w - 102, y + h - 54, 78, T(TxtOk), HoverDialogOk);
}

RECT NotificationOkBox(HWND hwnd) {
    RECT client = {};
    GetClientRect(hwnd, &client);
    int w = FromUi(client.right - client.left);
    int h = FromUi(client.bottom - client.top);
    return UiBox(w - 104, h - 54, 78, 34);
}

RECT NotificationSettingsBox(HWND hwnd) {
    RECT client = {};
    GetClientRect(hwnd, &client);
    int w = FromUi(client.right - client.left);
    int h = FromUi(client.bottom - client.top);
    return UiBox(w - 236, h - 54, 118, 34);
}

int HitTestNotificationControl(HWND hwnd, POINT pt) {
    RECT okRect = NotificationOkBox(hwnd);
    if (PtInRect(&okRect, pt)) return HoverNotifyOk;

    RECT settingsRect = NotificationSettingsBox(hwnd);
    if (PtInRect(&settingsRect, pt)) return HoverNotifySettings;

    return HoverNone;
}

void DrawNotificationChrome(HWND hwnd, HDC dc) {
    EnsureUiResources();
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(dc, &client, g_windowBrush);

    int w = FromUi(client.right - client.left);
    int h = FromUi(client.bottom - client.top);
    DrawSettingsCardPanel(dc, 22, 20, w - 44, h - 94);

    DrawInfoIcon(dc, 48, 54);

    std::wstring title = g_lastNotificationTitle.empty() ? T(TxtNotifyDialogTitle) : g_lastNotificationTitle;
    RECT titleRect = UiBox(98, 42, w - 130, 28);
    DrawTextLine(dc, title.c_str(), titleRect, g_sectionFont, g_theme.titleText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT bodyRect = UiBox(98, 78, w - 136, h - 178);
    DrawTextBlock(dc, g_lastNotificationBody, bodyRect, g_uiFont, g_theme.text,
                  DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);

    DrawRoundedAlphaFill(dc, 0, h - 74, w, 1, 1,
                         g_theme.dark ? Rgb(255, 255, 255) : Rgb(0, 0, 0),
                         g_theme.dark ? 16 : 12);
    RECT footerRect = UiBox(0, h - 73, w, 73);
    FillRect(dc, &footerRect, g_windowBrush);

    RECT settingsRect = NotificationSettingsBox(hwnd);
    int settingsX = FromUi(settingsRect.left);
    int settingsY = FromUi(settingsRect.top);
    DrawFooterButton(dc, settingsX, settingsY, 118, T(TxtMenuSettings), false,
                     IsHover(HoverNotifySettings), HoverNotifySettings);

    RECT okRect = NotificationOkBox(hwnd);
    int okX = FromUi(okRect.left);
    int okY = FromUi(okRect.top);
    DrawDialogButton(dc, okX, okY, 78, T(TxtOk), HoverNotifyOk);
}

void UpdateNotificationHover(HWND hwnd, POINT pt) {
    if (!g_trackingSettingsMouse) {
        TRACKMOUSEEVENT track = {};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = hwnd;
        TrackMouseEvent(&track);
        g_trackingSettingsMouse = true;
    }

    RememberSettingsMousePoint(pt);
    int nextHover = HitTestNotificationControl(hwnd, pt);
    if (nextHover != g_hoverControl) {
        g_hoverControl = nextHover;
        ArmSettingsAnimationTimer(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
    } else if (nextHover != HoverNone) {
        InvalidateRect(hwnd, NULL, FALSE);
    }
    SetCursor(LoadCursorW(NULL, nextHover != HoverNone ? IDC_HAND : IDC_ARROW));
}

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        RefreshUiDpi(hwnd);
        EnsureUiResources();
        ApplyModernWindowFrame(hwnd);
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON))));
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON))));
        ArmSettingsAnimationTimer(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps = {};
        HDC dc = BeginPaint(hwnd, &ps);
        ui_backbuffer::Draw(hwnd, dc, DrawNotificationChrome);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        UpdateNotificationHover(hwnd, pt);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        UpdateNotificationHover(hwnd, pt);
        int hit = HitTestNotificationControl(hwnd, pt);
        if (hit == HoverNotifyOk || hit == HoverNotifySettings) {
            g_pressedControl = hit;
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (g_pressedControl == HoverNotifyOk || g_pressedControl == HoverNotifySettings) {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            int pressed = g_pressedControl;
            int released = HitTestNotificationControl(hwnd, pt);
            g_pressedControl = 0;
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            if (pressed == released) {
                if (pressed == HoverNotifySettings) {
                    DestroyWindow(hwnd);
                    ShowSettingsWindow(g_mainWindow);
                } else {
                    DestroyWindow(hwnd);
                }
            }
            return 0;
        }
        break;
    case WM_TIMER:
        if (wParam == kSettingsAnimationTimer) {
            if (!UpdateSettingsAnimations(hwnd)) {
                KillTimer(hwnd, kSettingsAnimationTimer);
            }
            return 0;
        }
        break;
    case WM_MOUSELEAVE:
        g_trackingSettingsMouse = false;
        g_settingsMouseKnown = false;
        if (g_hoverControl == HoverNotifyOk || g_hoverControl == HoverNotifySettings) {
            g_hoverControl = HoverNone;
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT &&
            (g_hoverControl == HoverNotifyOk || g_hoverControl == HoverNotifySettings)) {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_SPACE) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kSettingsAnimationTimer);
        if (g_notificationWindow == hwnd) {
            g_notificationWindow = NULL;
        }
        if (g_hoverControl == HoverNotifyOk || g_hoverControl == HoverNotifySettings) {
            g_hoverControl = HoverNone;
        }
        if (g_pressedControl == HoverNotifyOk || g_pressedControl == HoverNotifySettings) {
            g_pressedControl = 0;
        }
        g_trackingSettingsMouse = false;
        g_settingsMouseKnown = false;
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ShowNotificationDialogWindow() {
    if (g_lastNotificationBody.empty()) return;

    if (g_notificationWindow) {
        SetForegroundWindow(g_notificationWindow);
        InvalidateRect(g_notificationWindow, NULL, TRUE);
        return;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = NotificationWndProc;
    wc.hInstance = g_instance;
    wc.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON));
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RefreshUiDpi(g_settingsWindow ? g_settingsWindow : g_mainWindow);
    EnsureUiResources();
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"HdrSdrBrightnessNotificationWindow";

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return;
    }

    DWORD style = WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_DLGMODALFRAME;
    const int clientW = 520;
    const int clientH = 250;
    SIZE windowSize = WindowSizeForClient(style, exStyle, Ui(clientW), Ui(clientH));

    std::wstring title = g_lastNotificationTitle.empty() ? T(TxtNotifyDialogTitle) : g_lastNotificationTitle;
    HWND owner = g_settingsWindow ? g_settingsWindow : g_mainWindow;
    g_notificationWindow = CreateWindowExW(exStyle, wc.lpszClassName, title.c_str(),
                                           style,
                                           CW_USEDEFAULT, CW_USEDEFAULT,
                                           windowSize.cx, windowSize.cy,
                                           owner, NULL, g_instance, NULL);
    if (!g_notificationWindow) return;

    CenterWindow(g_notificationWindow, windowSize.cx, windowSize.cy);
    ShowWindow(g_notificationWindow, SW_SHOW);
    SetForegroundWindow(g_notificationWindow);
    UpdateWindow(g_notificationWindow);
}

void RefreshSupportWindowStatus(HWND hwnd) {
    if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}

void ActivateSupporterCode(HWND hwnd) {
    std::wstring code = supporter_code::Normalize(g_supportCodeInput);

    if (!supporter_code::IsValid(code)) {
        g_supportStatus = T(TxtSupportInvalidCode);
        RefreshSupportWindowStatus(hwnd);
        return;
    }

    g_config.supporterCode = code;
    WriteStringValue(HKEY_CURRENT_USER, kConfigKey, L"SupporterCode", g_config.supporterCode);
    g_supportStatus = T(TxtSupportThanks);
    RefreshSupportWindowStatus(hwnd);
    if (g_settingsWindow) InvalidateRect(g_settingsWindow, NULL, FALSE);
}

RECT SupportDonateBox() {
    return UiBox(226, 188, 104, 32);
}

RECT SupportActivateBox() {
    return UiBox(340, 188, 104, 32);
}

RECT SupportCodeBox() {
    return UiBox(24, 110, 420, 32);
}

void InvalidateSupportCodeBox(HWND hwnd) {
    if (!hwnd) return;
    RECT rect = SupportCodeBox();
    InflateRect(&rect, Ui(2), Ui(2));
    InvalidateRect(hwnd, &rect, FALSE);
}

UINT SupportCaretBlinkInterval() {
    UINT blinkMs = GetCaretBlinkTime();
    if (blinkMs == INFINITE || blinkMs < 100) blinkMs = 530;
    return blinkMs;
}

void SetSupportCodeFocus(HWND hwnd, bool focused) {
    if (focused && GetFocus() != hwnd) {
        SetFocus(hwnd);
    }

    bool changed = g_supportCodeFocused != focused;
    g_supportCodeFocused = focused;
    g_supportCaretVisible = focused;
    if (focused) {
        SetTimer(hwnd, kSupportCaretTimer, SupportCaretBlinkInterval(), NULL);
    } else {
        KillTimer(hwnd, kSupportCaretTimer);
    }

    if (changed) {
        ArmSettingsAnimationTimer(hwnd);
    }
    InvalidateSupportCodeBox(hwnd);
}

bool SetSupporterCodeInputFromText(const std::wstring& value) {
    std::wstring normalized = supporter_code::Normalize(value);
    if (normalized.empty()) return false;
    if (normalized.size() > kSupporterCodeMaxLength) normalized.resize(kSupporterCodeMaxLength);
    g_supportCodeInput = normalized;
    return !normalized.empty();
}

bool PasteSupporterCodeFromClipboard(HWND hwnd) {
    if (!OpenClipboard(hwnd)) return false;

    bool pasted = false;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data) {
        const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(data));
        if (text) {
            pasted = SetSupporterCodeInputFromText(text);
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return pasted;
}

bool IsSupportHoverControl(int control) {
    return control == HoverSupportDonate ||
           control == HoverSupportActivate ||
           control == HoverSupportCode;
}

void ClearSupportInteractionState(HWND hwnd) {
    KillTimer(hwnd, kSupportCaretTimer);
    if (GetCapture() == hwnd) ReleaseCapture();
    g_supportPressedControl = 0;
    g_supportCodeFocused = false;
    g_supportCaretVisible = false;
    if (IsSupportHoverControl(g_hoverControl)) {
        g_hoverControl = HoverNone;
    }
    g_trackingSettingsMouse = false;
    g_settingsMouseKnown = false;
}

void IgnoreSettingsMouseBriefly() {
    g_ignoreSettingsMouseUntil = GetTickCount() + 350;
}

bool ShouldIgnoreSettingsMouse() {
    return static_cast<int>(g_ignoreSettingsMouseUntil - GetTickCount()) > 0;
}

int HitTestSupportControl(POINT pt) {
    RECT donate = SupportDonateBox();
    if (PtInRect(&donate, pt)) return HoverSupportDonate;
    RECT activate = SupportActivateBox();
    if (PtInRect(&activate, pt)) return HoverSupportActivate;
    RECT code = SupportCodeBox();
    if (PtInRect(&code, pt)) return HoverSupportCode;
    return HoverNone;
}

void DrawSupportWindowChrome(HWND hwnd, HDC dc) {
    EnsureUiResources();
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(dc, &client, g_windowBrush);

    DrawWindowGlassBackdrop(dc, FromUi(client.bottom - client.top));
    DrawSettingsCardPanel(dc, 16, 16, 436, 154);
    DrawAppMark(dc, 34, 34);

    RECT title = UiBox(84, 30, 330, 28);
    DrawTextLine(dc, T(TxtSupportAuthor), title, g_sectionFont, g_theme.titleText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT subtitle = UiBox(84, 60, 340, 46);
    DrawTextBlock(dc, T(TxtSupportAuthorSubtitle), subtitle, g_uiFont, g_theme.text,
                  DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);

    RECT label = UiBox(24, 86, 180, 22);
    DrawTextLine(dc, T(TxtSupportCodeLabel), label, g_smallFont, g_theme.mutedText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    int codeAmount = std::max(InteractionPercent(HoverSupportCode), g_supportCodeFocused ? 70 : 0);
    DrawLiquidGlassPanel(dc, 24, 110, 420, 32, 8,
                         BlendColor(g_theme.control, g_theme.controlHover, codeAmount),
                         g_supportCodeFocused ? g_theme.primaryBorderHover : g_theme.controlBorderHover,
                         HoverSupportCode);
    std::wstring codeText = g_supportCodeInput.empty() && !g_supportCodeFocused
                                ? std::wstring(L"HDRSDR-XXXXXXXX-XXXX")
                                : g_supportCodeInput;
    RECT codeRect = UiBox(36, 110, 396, 32);
    DrawTextLine(dc, codeText.c_str(), codeRect, g_uiFont,
                 g_supportCodeInput.empty() && !g_supportCodeFocused ? g_theme.disabledText : g_theme.titleText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (g_supportCodeFocused && g_supportCaretVisible) {
        int caretX = 38 + std::min(370, TextWidthLogical(dc, g_supportCodeInput.c_str(), g_uiFont));
        DrawSolidLogicalRect(dc, caretX, 118, 2, 16, g_theme.primaryHover);
    }

    std::wstring status = !g_supportStatus.empty() ? g_supportStatus
                        : (HasSupporterBadge() ? std::wstring(T(TxtSupportThanks)) : std::wstring());
    RECT statusRect = UiBox(24, 149, 420, 22);
    DrawTextLine(dc, status.c_str(), statusRect, g_smallFont,
                supporter_code::IsValid(g_config.supporterCode) ? Rgb(34, 197, 94) : g_theme.mutedText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    DrawFooterButton(dc, 226, 188, 104, T(TxtSupportDonate), false,
                     IsHover(HoverSupportDonate), HoverSupportDonate);
    DrawFooterButton(dc, 340, 188, 104, T(TxtSupportActivate), true,
                     IsHover(HoverSupportActivate), HoverSupportActivate);
}

#if defined(__GNUC__)
LRESULT CALLBACK SupportWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) __attribute__((unused));
#endif
LRESULT CALLBACK SupportWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        RefreshUiDpi(hwnd);
        EnsureUiResources();
        ApplyModernWindowFrame(hwnd);
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON))));
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON))));
        g_supportCodeInput = g_config.supporterCode;
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps = {};
        HDC dc = BeginPaint(hwnd, &ps);
        ui_backbuffer::Draw(hwnd, dc, DrawSupportWindowChrome);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RememberSettingsMousePoint(pt);
        if (!g_trackingSettingsMouse) {
            TRACKMOUSEEVENT track = {};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd;
            TrackMouseEvent(&track);
            g_trackingSettingsMouse = true;
        }
        int nextHover = HitTestSupportControl(pt);
        if (nextHover != g_hoverControl) {
            g_hoverControl = nextHover;
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (nextHover != HoverNone) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        SetCursor(LoadCursorW(NULL, nextHover != HoverNone ? IDC_HAND : IDC_ARROW));
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        int hit = HitTestSupportControl(pt);
        SetSupportCodeFocus(hwnd, hit == HoverSupportCode);
        if (hit == HoverSupportDonate || hit == HoverSupportActivate) {
            g_supportPressedControl = hit;
            SetCapture(hwnd);
            ArmSettingsAnimationTimer(hwnd);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_LBUTTONUP:
        if (g_supportPressedControl == HoverSupportDonate || g_supportPressedControl == HoverSupportActivate) {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            int pressed = g_supportPressedControl;
            int released = HitTestSupportControl(pt);
            g_supportPressedControl = 0;
            if (GetCapture() == hwnd) ReleaseCapture();
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            if (pressed == released) {
                if (pressed == HoverSupportDonate) {
                    ShellExecuteW(hwnd, L"open", kStoreSupportUrl, NULL, NULL, SW_SHOWNORMAL);
                } else if (pressed == HoverSupportActivate) {
                    ActivateSupporterCode(hwnd);
                }
            }
            return 0;
        }
        break;
    case WM_MOUSELEAVE:
        g_trackingSettingsMouse = false;
        g_settingsMouseKnown = false;
        if (IsSupportHoverControl(g_hoverControl)) {
            g_hoverControl = HoverNone;
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_TIMER:
        if (wParam == kSettingsAnimationTimer) {
            if (!UpdateSettingsAnimations(hwnd)) {
                KillTimer(hwnd, kSettingsAnimationTimer);
            }
            return 0;
        }
        if (wParam == kSupportCaretTimer) {
            if (g_supportCodeFocused) {
                g_supportCaretVisible = !g_supportCaretVisible;
                InvalidateSupportCodeBox(hwnd);
            } else {
                KillTimer(hwnd, kSupportCaretTimer);
            }
            return 0;
        }
        break;
    case WM_SETFOCUS:
        if (g_supportCodeFocused) {
            g_supportCaretVisible = true;
            SetTimer(hwnd, kSupportCaretTimer, SupportCaretBlinkInterval(), NULL);
            InvalidateSupportCodeBox(hwnd);
        }
        return 0;
    case WM_CHAR: {
        if (!g_supportCodeFocused) break;
        wchar_t ch = static_cast<wchar_t>(wParam);
        bool edited = false;
        if (ch == L'\b') {
            if (!g_supportCodeInput.empty()) {
                g_supportCodeInput.erase(g_supportCodeInput.size() - 1);
                edited = true;
            }
        } else if (ch == L'\r') {
            ActivateSupporterCode(hwnd);
            return 0;
        } else if (supporter_code::IsAllowedCharacter(ch) && g_supportCodeInput.size() < kSupporterCodeMaxLength) {
            wchar_t normalized = ch;
            if (normalized >= L'a' && normalized <= L'z') normalized = static_cast<wchar_t>(normalized - L'a' + L'A');
            g_supportCodeInput.push_back(normalized);
            edited = true;
        }
        if (edited) {
            g_supportStatus.clear();
            g_supportCaretVisible = true;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (g_supportCodeFocused &&
            ((wParam == L'V' && (GetKeyState(VK_CONTROL) & 0x8000)) ||
             (wParam == VK_INSERT && (GetKeyState(VK_SHIFT) & 0x8000)))) {
            if (PasteSupporterCodeFromClipboard(hwnd)) {
                g_supportStatus.clear();
                g_supportCaretVisible = true;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        if (wParam == VK_RETURN) {
            ActivateSupporterCode(hwnd);
            return 0;
        }
        break;
    case WM_PASTE:
        if (g_supportCodeFocused && PasteSupporterCodeFromClipboard(hwnd)) {
            g_supportStatus.clear();
            g_supportCaretVisible = true;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_KILLFOCUS:
        SetSupportCodeFocus(hwnd, false);
        return 0;
    case WM_CAPTURECHANGED:
        if (reinterpret_cast<HWND>(lParam) != hwnd && g_supportPressedControl != 0) {
            g_supportPressedControl = 0;
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_CLOSE:
        IgnoreSettingsMouseBriefly();
        ClearSupportInteractionState(hwnd);
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        IgnoreSettingsMouseBriefly();
        ClearSupportInteractionState(hwnd);
        if (g_supportWindow == hwnd) {
            g_supportWindow = NULL;
            g_supportOwnerWindow = NULL;
            g_supportStatus.clear();
            g_supportCodeInput.clear();
        }
        if (g_settingsWindow) InvalidateRect(g_settingsWindow, NULL, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ShowSupportWindow(HWND owner) {
    if (!IsSupportFeatureAvailable()) return;
    ShellExecuteW(owner ? owner : g_mainWindow, L"open", kStoreSupportUrl, NULL, NULL, SW_SHOWNORMAL);
}

void DrawHeroCard(HDC dc, const SettingsLayout& layout) {
    const int x = layout.cardX;
    const int y = layout.heroTop;
    const int w = layout.cardW;
    DrawSettingsCardPanel(dc, x, y, w, layout.heroH);

    int displayedBrightness = -1;
    bool displayedNight = false;
    bool hasDisplayedState = GetDisplayedBrightnessState(&displayedBrightness, &displayedNight);
    std::wstring mode = hasDisplayedState
                            ? std::wstring(displayedNight ? T(TxtNight) : T(TxtDay))
                            : std::wstring(T(TxtStarting));
    std::wstring level = hasDisplayedState ? PercentLabel(displayedBrightness)
                                           : std::wstring(T(TxtUnknownPercent));
    std::wstring headline = mode + L"  " + level;

    RECT labelRect = UiBox(x + kSettingsCardPadding, y + kSettingsCardTopPadding, 230, 22);
    DrawTextLine(dc, T(TxtCurrentState), labelRect, g_sectionFont, g_theme.titleText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT headlineRect = UiBox(x + kSettingsCardPadding, y + 36, 360, 34);
    DrawTextLine(dc, headline.c_str(), headlineRect, g_heroFont, g_theme.titleText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    std::wstring status = HeroStatusText();
    RECT statusRect = UiBox(x + kSettingsCardPadding, y + 72, 390, 22);
    DrawTextLine(dc, status.c_str(), statusRect, g_smallFont, g_theme.mutedText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    DrawStatusPill(dc, x + w - kSettingsCardPadding - 148, y + kSettingsCardTopPadding, 148, HdrPillText(),
                   g_lastHdrTargetCount > 0 ? Rgb(34, 197, 94) : Rgb(245, 158, 11));
    DrawStatusPill(dc, x + w - kSettingsCardPadding - 148, y + 48, 148,
                   g_settingsDraft.autoRestoreManualChanges ? T(TxtRestoreOn)
                                                            : T(TxtRestoreOff),
                   g_settingsDraft.autoRestoreManualChanges ? Rgb(34, 197, 94) : Rgb(148, 163, 184));

    if (ShouldShowHdrCalibrationCallout()) {
        bool calibrationHover = IsHover(HoverHdrCalibration);
        bool dismissHover = IsHover(HoverHdrCalibrationDismiss);
        bool calibrationPressed = g_pressedControl == HoverHdrCalibration;
        bool dismissPressed = g_pressedControl == HoverHdrCalibrationDismiss;
        int calloutX = x + kSettingsCardPadding;
        int calloutY = HdrCalibrationCalloutY(layout);
        int calloutW = w - kSettingsCardPadding * 2;
        COLORREF fill = BlendColor(g_theme.elevated, g_theme.control, calibrationHover ? 18 : 0);
        COLORREF border = BlendColor(g_theme.elevated, g_theme.controlBorderHover,
                                     calibrationHover || dismissHover ? 30 : 0);
        DrawLiquidGlassPanel(dc, calloutX, calloutY, calloutW, 28, 14, fill, border);
        DrawCircleFill(dc, calloutX + 12, calloutY + 10, 8, g_theme.primary, g_theme.primary);

        RECT linkRect = HdrCalibrationLinkBox(layout);
        RECT dismissRect = HdrCalibrationDismissBox(layout);
        RECT hintRect = UiBox(calloutX + 26, calloutY, FromUi(linkRect.left) - calloutX - 38, 28);
        DrawTextLine(dc, T(TxtHdrCalibrationHint), hintRect, g_smallFont, g_theme.mutedText,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT linkTextRect = linkRect;
        linkTextRect.top += Ui(calibrationPressed ? 1 : 0);
        COLORREF linkColor = calibrationHover ? g_theme.primaryHover : g_theme.primary;
        DrawTextLine(dc, T(TxtHdrCalibrationLink), linkTextRect, g_smallFont, linkColor,
                     DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (calibrationHover || calibrationPressed) {
            int underlineW = std::min(FromUi(linkRect.right - linkRect.left),
                                      TextWidthLogical(dc, T(TxtHdrCalibrationLink), g_smallFont));
            DrawSolidLogicalRect(dc, FromUi(linkRect.right) - underlineW,
                                 calloutY + 21 + (calibrationPressed ? 1 : 0),
                                 underlineW, 1, linkColor);
        }

        if (dismissHover || dismissPressed) {
            DrawLiquidGlassPanel(dc, FromUi(dismissRect.left), FromUi(dismissRect.top), 24, 24, 6,
                                 g_theme.controlHover, g_theme.controlHover, HoverHdrCalibrationDismiss);
        }
        DrawCloseIcon(dc, FromUi(dismissRect.left) + 8,
                      FromUi(dismissRect.top) + 8 + (dismissPressed ? 1 : 0),
                      8, dismissHover ? g_theme.text : g_theme.mutedText);
    }
}

void DrawSettingsChrome(HWND hwnd, HDC dc) {
    EnsureUiResources();
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(dc, &client, g_windowBrush);
    DrawSettingsTitleBar(hwnd, dc);

    POINT contentOrigin = {};
    SetViewportOrgEx(dc, 0, Ui(kSettingsTitleBarHeight), &contentOrigin);

    int visibleHeight = SettingsContentClientHeight(hwnd);
    DrawWindowGlassBackdrop(dc, visibleHeight);
    const int titlePadY = kSettingsCardTopPadding;
    bool canFollow = CanFollowWindowsNightLight();
    bool useSystem = g_settingsDraft.followNightLight && canFollow;
    SettingsLayout layout = BuildSettingsLayout(useSystem);
    const int sectionX = layout.cardX + kSettingsCardPadding;
    const int rowX = sectionX;
    const int contentW = layout.cardW - kSettingsCardPadding * 2;
    const int controlX = layout.cardX + layout.cardW - kSettingsCardPadding - 44;
    g_settingsScrollY = ClampInt(g_settingsScrollY, 0, std::max(0, layout.footerY - (visibleHeight - kSettingsFooterAreaHeight)));

    POINT oldOrigin = {};
    int appearOffset = (1000 - ClampInt(g_settingsWindowAnim, 0, 1000)) / 80;
    SetViewportOrgEx(dc, 0, Ui(kSettingsTitleBarHeight) - Ui(g_settingsScrollY) + Ui(appearOffset), &oldOrigin);

    // 裁剪区域：防止滚动内容绘制到标题栏区域（device y < kSettingsTitleBarHeight）
    HRGN contentClip = CreateRectRgn(0, Ui(kSettingsTitleBarHeight), client.right, client.bottom);
    SelectClipRgn(dc, contentClip);
    DeleteObject(contentClip);

    DrawAppMark(dc, layout.headerIconX, layout.headerIconY);
    RECT title = UiBox(layout.headerTitleX, layout.headerTitleY, 500, 30);
    DrawTextLine(dc, T(TxtDisplayName), title, g_titleFont, g_theme.titleText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT subtitle = UiBox(layout.headerTitleX, layout.headerSubtitleY, 500, 22);
    DrawTextLine(dc, T(TxtSettingsSubtitle), subtitle, g_smallFont, g_theme.mutedText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (IsSupportFeatureAvailable()) {
        if (HasSupporterBadge()) {
            DrawSupporterBadge(dc, SupporterBadgeLeft(dc), 29);
        } else {
            DrawSupportButton(dc, SupportButtonLeft(dc), kStoreSupportButtonY);
        }
    }

    DrawHeroCard(dc, layout);

    DrawSettingsCardPanel(dc, layout.cardX, layout.brightnessTop, layout.cardW, layout.brightnessH);
    DrawSettingsCardPanel(dc, layout.cardX, layout.switchTop, layout.cardW, layout.switchH);
    DrawSettingsCardPanel(dc, layout.cardX, layout.appearanceTop, layout.cardW, layout.appearanceH);
    DrawSettingsCardPanel(dc, layout.cardX, layout.behaviorTop, layout.cardW, layout.behaviorH);

    int sectionY = layout.brightnessTop + titlePadY;
    int row1 = layout.brightnessRow1;
    int row2 = layout.brightnessRow2;
    RECT section = UiBox(sectionX, sectionY, 240, 24);
    DrawTextLine(dc, T(TxtBrightnessGroup), section, g_sectionFont, g_theme.titleText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawSdrHdrComparison(dc, layout);
    int sliderX = BrightnessSliderX(layout);
    int sliderW = BrightnessSliderWidth(layout);
    int valueX = BrightnessValueX(layout);
    DrawSettingRowText(dc, T(TxtDay), rowX, row1, 120);
    DrawSlider(dc, sliderX, row1 - 2, sliderW, g_settingsDraft.dayBrightness, IsHover(HoverDaySlider), HoverDaySlider);
    DrawValuePill(dc, valueX, row1 - 9, g_settingsDraft.dayBrightness);
    DrawCardSeparator(dc, sectionX, row2 - 11, contentW);
    DrawSettingRowText(dc, T(TxtNight), rowX, row2, 120);
    DrawSlider(dc, sliderX, row2 - 2, sliderW, g_settingsDraft.nightBrightness, IsHover(HoverNightSlider), HoverNightSlider);
    DrawValuePill(dc, valueX, row2 - 9, g_settingsDraft.nightBrightness);

    sectionY = layout.switchTop + titlePadY;
    section = UiBox(sectionX, sectionY, 240, 24);
    DrawTextLine(dc, T(TxtSwitchMode), section, g_sectionFont, g_theme.titleText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawSettingsRowHoverBox(dc, layout.cardX + 12, layout.switchModeY - 4, layout.cardW - 24, 40,
                            std::max(InteractionPercent(HoverSwitchBase),
                                     InteractionPercent(HoverSwitchBase + 1)));
    DrawSegmentedSwitchMode(dc, layout, canFollow);
    if (useSystem) {
        RECT hintRect = UiBox(rowX, layout.switchHintY, 500, 28);
        DrawTextLine(dc, T(TxtFollowSystemHint), hintRect, g_uiFont, g_theme.mutedText,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        DrawSettingsRowHoverBox(dc, layout.cardX + 12, layout.switchNightY - 5, layout.cardW - 24, 36,
                                std::max(InteractionPercent(HoverNightMinus),
                                         InteractionPercent(HoverNightPlus)));
        DrawSettingsRowHoverBox(dc, layout.cardX + 12, layout.switchDayY - 5, layout.cardW - 24, 36,
                                std::max(InteractionPercent(HoverDayMinus),
                                         InteractionPercent(HoverDayPlus)));
        DrawTimeStepper(dc, layout, T(TxtNightStarts), layout.switchNightY, g_settingsDraft.nightStartHour, g_settingsDraft.nightStartMinute,
                        IsHover(HoverNightMinus), IsHover(HoverNightPlus), HoverNightMinus, HoverNightPlus);
        DrawCardSeparator(dc, sectionX, layout.switchDayY - 10, contentW);
        DrawTimeStepper(dc, layout, T(TxtDayStarts), layout.switchDayY, g_settingsDraft.dayStartHour, g_settingsDraft.dayStartMinute,
                        IsHover(HoverDayMinus), IsHover(HoverDayPlus), HoverDayMinus, HoverDayPlus);
    }

    sectionY = layout.appearanceTop + titlePadY;
    row1 = layout.appearanceRow1;
    section = UiBox(sectionX, sectionY, 240, 24);
    DrawTextLine(dc, T(TxtAppearanceGroup), section, g_sectionFont, g_theme.titleText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawSettingsRowHoverBox(dc, layout.cardX + 12, row1 - 6, layout.cardW - 24, 42,
                            InteractionPercent(HoverLanguageButton));
    DrawSettingRowText(dc, T(TxtLanguage), rowX, row1, 330);
    DrawLanguageSelector(dc, layout);

    sectionY = layout.behaviorTop + titlePadY;
    row1 = layout.behaviorRow1;
    row2 = layout.behaviorRow2;
    section = UiBox(sectionX, sectionY, 240, 24);
    DrawTextLine(dc, T(TxtBehaviorGroup), section, g_sectionFont, g_theme.titleText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawSettingsRowHoverBox(dc, layout.cardX + 12, row1 - 5, layout.cardW - 24, 36,
                            InteractionPercent(HoverAutoRestore));
    if (IsStartupFeatureAvailable()) {
        DrawSettingsRowHoverBox(dc, layout.cardX + 12, row2 - 5, layout.cardW - 24, 36,
                                InteractionPercent(HoverStartup));
    }
    DrawSettingRowText(dc, T(TxtAutoRestoreManual), rowX, row1, 330);
    DrawToggle(dc, controlX, row1 + 2, g_settingsDraft.autoRestoreManualChanges, IsHover(HoverAutoRestore), HoverAutoRestore);
    if (IsStartupFeatureAvailable()) {
        DrawCardSeparator(dc, sectionX, row2 - 10, contentW);
        DrawSettingRowText(dc, T(TxtStartWithWindows), rowX, row2, 330);
        DrawToggle(dc, controlX, row2 + 2, g_settingsDraft.startWithWindows, IsHover(HoverStartup), HoverStartup);
    }
    DrawCardSeparator(dc, sectionX, layout.hotkeyRow1 - 10, contentW);
    DrawSettingRowText(dc, T(TxtScreenshotHotkeys), rowX, layout.hotkeyRow1, 330);
    DrawHotkeyPill(dc, layout.languageSegmentX, layout.hotkeyRow1 - 5, layout.languageSegmentW,
                   T(TxtHotkeyGroup), false, IsHover(HoverHotkeySettings));

    // 移除裁剪区域，恢复正常绘制（footer/scrollbar/tooltip 需要全区域访问）
    SelectClipRgn(dc, NULL);
    DrawLanguageDropdown(dc, layout);
    SetViewportOrgEx(dc, oldOrigin.x, oldOrigin.y, NULL);
    DrawSettingsFooter(dc, visibleHeight);
    DrawSettingsScrollbar(dc, layout, visibleHeight);
    if (IsSupportFeatureAvailable() && !g_settingsInfoDialogOpen && !g_hotkeyDialogOpen && IsHover(HoverSupporterBadge)) {
        DrawSupporterTooltip(dc, 408, 66);
    }
    DrawSettingsInfoDialog(dc, hwnd);
    DrawHotkeyDialog(dc, hwnd);
    SetViewportOrgEx(dc, contentOrigin.x, contentOrigin.y, NULL);
    DrawSettingsTitleTooltip(dc);
}

void ApplySettingsDraft(HWND hwnd, bool closeWindow) {
    if (g_settingsDraft.followNightLight && !CanFollowWindowsNightLight()) {
        g_settingsDraft.followNightLight = false;
    }
    ClearSettingsBrightnessPreview();
    std::wstring supporterCode = g_config.supporterCode;
    g_config = g_settingsDraft;
    g_config.supporterCode = supporterCode;
    g_brightnessConfigReady = true;
    SaveConfig();
    RegisterAppHotkeys();
    PostMessageW(g_mainWindow, kApplyMessage, TRUE, 0);
    if (closeWindow) {
        DestroyWindow(hwnd);
    } else {
        UpdateSettingsWindowTitle(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateTrayTip();
    }
}

void SyncStoreStartupStateToSettingsDraft(HWND hwnd) {
    if (!UseStoreStartupIntegration() || !IsStartupFeatureAvailable()) return;

    bool startupEnabled = IsStartupEnabled();
    if (g_config.startWithWindows == startupEnabled &&
        (!g_settingsDraftActive || g_settingsDraft.startWithWindows == startupEnabled)) {
        return;
    }

    g_config.startWithWindows = startupEnabled;
    if (g_settingsDraftActive) {
        g_settingsDraft.startWithWindows = startupEnabled;
    }
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"StartWithWindows", startupEnabled ? 1 : 0);
    if (hwnd) {
        UpdateSettingsWindowTitle(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateTrayTip();
    }
}

int SetDraftBrightnessFromPoint(HWND hwnd, POINT pt, int id) {
    SettingsLayout layout = BuildSettingsLayout(SettingsDraftUsesSystemSwitching());
    int value = SliderValueFromPoint(pt, BrightnessSliderX(layout), BrightnessSliderWidth(layout));
    bool changed = false;
    if (id == kIdDayBrightness) {
        changed = g_settingsDraft.dayBrightness != value;
        g_settingsDraft.dayBrightness = value;
    } else if (id == kIdNightBrightness) {
        changed = g_settingsDraft.nightBrightness != value;
        g_settingsDraft.nightBrightness = value;
    }
    if (changed) {
        InvalidateRect(hwnd, NULL, FALSE);
    }
    return value;
}

void ApplySettingsLanguageChoice(HWND hwnd, int language) {
    language = NormalizeLanguageChoice(language);
    if (g_settingsDraft.language == language) return;

    g_settingsDraft.language = language;
    CleanupFontResources();
    EnsureUiResources();
    UpdateSettingsWindowTitle(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

void ShowSettingsInfoDialog(HWND hwnd) {
    g_languageDropdownOpen = false;
    g_settingsInfoDialogOpen = true;
    g_pressedControl = 0;
    ArmSettingsAnimationTimer(hwnd);
    UpdateHdrPreviewWindow(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

void CloseSettingsInfoDialog(HWND hwnd) {
    if (!g_settingsInfoDialogOpen) return;
    g_settingsInfoDialogOpen = false;
    g_pressedControl = 0;
    if (g_hoverControl == HoverDialogOk || g_hoverControl == HoverDialogClose || g_hoverControl == HoverDialogLink) {
        g_hoverControl = HoverNone;
    }
    ArmSettingsAnimationTimer(hwnd);
    UpdateHdrPreviewWindow(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
}

void ShowHotkeyDialog(HWND hwnd) {
    g_languageDropdownOpen = false;
    g_settingsInfoDialogOpen = false;
    g_hotkeyDialogOpen = true;
    g_pressedControl = 0;
    ArmSettingsAnimationTimer(hwnd);
    UpdateHdrPreviewWindow(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

void CloseHotkeyDialog(HWND hwnd) {
    if (!g_hotkeyDialogOpen) return;
    g_hotkeyDialogOpen = false;
    g_pressedControl = 0;
    if (g_recordingHotkey != 0) {
        g_recordingHotkey = 0;
        RegisterAppHotkeys();
    }
    if (g_hoverControl == HoverDialogOk || g_hoverControl == HoverDialogClose ||
        g_hoverControl == HoverScreenshotHotkey || g_hoverControl == HoverFullscreenHotkey) {
        g_hoverControl = HoverNone;
    }
    ArmSettingsAnimationTimer(hwnd);
    UpdateHdrPreviewWindow(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
}

void StartHotkeyRecording(HWND hwnd, int control) {
    if (control != HoverScreenshotHotkey && control != HoverFullscreenHotkey) return;
    g_recordingHotkey = control;
    UnregisterAppHotkeys();
    ArmSettingsAnimationTimer(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

void ApplyRecordedHotkey(int target, UINT mod, UINT vk) {
    if (target == HoverScreenshotHotkey) {
        g_config.screenshotHotkeyMod = mod;
        g_config.screenshotHotkeyVk = vk;
        if (g_settingsDraftActive) {
            g_settingsDraft.screenshotHotkeyMod = mod;
            g_settingsDraft.screenshotHotkeyVk = vk;
        }
        return;
    }

    if (target == HoverFullscreenHotkey) {
        g_config.fullscreenHotkeyMod = mod;
        g_config.fullscreenHotkeyVk = vk;
        if (g_settingsDraftActive) {
            g_settingsDraft.fullscreenHotkeyMod = mod;
            g_settingsDraft.fullscreenHotkeyVk = vk;
        }
    }
}

void OpenNightLightSettings(HWND hwnd) {
    ShellExecuteW(hwnd, L"open", L"ms-settings:nightlight", NULL, NULL, SW_SHOWNORMAL);
}

void HandleSettingsClick(HWND hwnd, POINT pt) {
    if (g_settingsInfoDialogOpen || g_hotkeyDialogOpen) {
        return;
    }

    int footerHover = HitTestSettingsFooterControl(hwnd, pt);
    if (footerHover == HoverOk) {
        ApplySettingsDraft(hwnd, true);
        return;
    }
    if (footerHover == HoverApply) {
        ApplySettingsDraft(hwnd, false);
        return;
    }
    if (footerHover == HoverCancel) {
        RestoreSettingsBrightnessPreviewIfNeeded();
        DestroyWindow(hwnd);
        return;
    }

    pt = SettingsContentPoint(pt);
    bool canFollow = CanFollowWindowsNightLight();
    bool useSystem = g_settingsDraft.followNightLight && canFollow;
    SettingsLayout layout = BuildSettingsLayout(useSystem);

    if (g_languageDropdownOpen) {
        int option = LanguageDropdownOptionFromPoint(pt, layout);
        if (option >= 0) {
            const LanguageOption* languages = LanguageOptions();
            g_languageDropdownOpen = false;
            UpdateHdrPreviewWindow(hwnd);
            ArmSettingsAnimationTimer(hwnd);
            ApplySettingsLanguageChoice(hwnd, languages[option].id);
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }

        g_languageDropdownOpen = false;
        UpdateHdrPreviewWindow(hwnd);
        ArmSettingsAnimationTimer(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
    }

    if (PtInUiBox(pt, layout.languageSegmentX, layout.languageSegmentY, layout.languageSegmentW, layout.languageSegmentH)) {
        g_languageDropdownOpen = !g_languageDropdownOpen;
        UpdateHdrPreviewWindow(hwnd);
        ArmSettingsAnimationTimer(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    int sliderX = BrightnessSliderX(layout);
    int sliderW = BrightnessSliderWidth(layout);
    if (PtInUiBox(pt, sliderX - 20, layout.brightnessRow1 - 22, sliderW + 40, 40)) {
        g_draggingBrightnessId = kIdDayBrightness;
        SetCapture(hwnd);
        int value = SetDraftBrightnessFromPoint(hwnd, pt, g_draggingBrightnessId);
        ApplySettingsBrightnessPreview(hwnd, g_draggingBrightnessId, value);
        return;
    }
    if (PtInUiBox(pt, sliderX - 20, layout.brightnessRow2 - 22, sliderW + 40, 40)) {
        g_draggingBrightnessId = kIdNightBrightness;
        SetCapture(hwnd);
        int value = SetDraftBrightnessFromPoint(hwnd, pt, g_draggingBrightnessId);
        ApplySettingsBrightnessPreview(hwnd, g_draggingBrightnessId, value);
        return;
    }

    if (PtInUiBox(pt, layout.switchModeX, layout.switchModeY, layout.switchModeW, layout.switchModeH)) {
        int localX = pt.x - Ui(layout.switchModeX);
        int segment = ClampInt(localX / std::max(1, Ui(layout.switchModeW / 2)), 0, 1);
        if (segment == 0) {
            if (canFollow) {
                g_settingsDraft.followNightLight = true;
            } else {
                ShowSettingsInfoDialog(hwnd);
                g_settingsDraft.followNightLight = false;
            }
        } else {
            g_settingsDraft.followNightLight = false;
        }
        ResizeSettingsWindowToLayout(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    if (ShouldShowHdrCalibrationCallout()) {
        RECT linkRect = HdrCalibrationLinkBox(layout);
        if (PtInRect(&linkRect, pt)) {
            OpenHdrCalibration(hwnd);
            return;
        }
        RECT dismissRect = HdrCalibrationDismissBox(layout);
        if (PtInRect(&dismissRect, pt)) {
            DismissHdrCalibrationCallout(hwnd);
            return;
        }
    }

    if (PtInUiBox(pt, layout.cardX + 12, layout.behaviorRow1 - 5, layout.cardW - 24, 36)) {
        g_settingsDraft.autoRestoreManualChanges = !g_settingsDraft.autoRestoreManualChanges;
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (IsStartupFeatureAvailable() &&
        PtInUiBox(pt, layout.cardX + 12, layout.behaviorRow2 - 5, layout.cardW - 24, 36)) {
        g_settingsDraft.startWithWindows = !g_settingsDraft.startWithWindows;
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (PtInUiBox(pt, layout.languageSegmentX, layout.hotkeyRow1 - 4,
                  layout.languageSegmentW, 32)) {
        ShowHotkeyDialog(hwnd);
        return;
    }

    if (!useSystem && PtInUiBox(pt, TimeStepperMinusX(layout), layout.switchNightY - 4, 28, 28)) {
        night_mode::AddMinutesToTime(&g_settingsDraft.nightStartHour, &g_settingsDraft.nightStartMinute, -30);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (!useSystem && PtInUiBox(pt, TimeStepperPlusX(layout), layout.switchNightY - 4, 28, 28)) {
        night_mode::AddMinutesToTime(&g_settingsDraft.nightStartHour, &g_settingsDraft.nightStartMinute, 30);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (!useSystem && PtInUiBox(pt, TimeStepperMinusX(layout), layout.switchDayY - 4, 28, 28)) {
        night_mode::AddMinutesToTime(&g_settingsDraft.dayStartHour, &g_settingsDraft.dayStartMinute, -30);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (!useSystem && PtInUiBox(pt, TimeStepperPlusX(layout), layout.switchDayY - 4, 28, 28)) {
        night_mode::AddMinutesToTime(&g_settingsDraft.dayStartHour, &g_settingsDraft.dayStartMinute, 30);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

}

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        RefreshUiDpi(hwnd);
        EnsureUiResources();
        LoadConfig(true);
        g_settingsDraft = g_config;
        if (!CanFollowWindowsNightLight()) {
            g_settingsDraft.followNightLight = false;
        }
        g_settingsDraftActive = true;
        ClearSettingsBrightnessPreview();
        g_settingsInfoDialogOpen = false;
        g_hotkeyDialogOpen = false;
        g_pressedControl = 0;
        g_settingsScrollY = 0;
        g_hoverControl = HoverNone;
        g_settingsWindowAnim = 0;
        g_settingsDialogAnim = 0;
        g_settingsDropdownAnim = 0;
        for (int i = 0; i < kAnimationSlotCount; ++i) {
            g_controlAnim[i] = 0;
        }
        g_trackingSettingsMouse = false;
        g_settingsMouseKnown = false;
        UpdateSettingsWindowTitle(hwnd);
        ApplyModernWindowFrame(hwnd);
        CreateHdrPreviewWindow(hwnd);
        UpdateHdrPreviewWindow(hwnd);
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON))));
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON))));
        ArmSettingsAnimationTimer(hwnd);
        return 0;
    }
    case WM_DPICHANGED: {
        RefreshUiDpi(hwnd);
        EnsureUiResources();
        RECT* suggested = reinterpret_cast<RECT*>(lParam);
        if (suggested) {
            SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        } else {
            ResizeSettingsWindowToLayout(hwnd);
        }
        HdrPreviewResetDevice(&g_hdrPreview);
        UpdateHdrPreviewWindow(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_ACTIVATE:
        if (LOWORD(wParam) != WA_INACTIVE) {
            SyncStoreStartupStateToSettingsDraft(hwnd);
        }
        break;
    case WM_SHOWWINDOW:
        if (wParam) {
            SyncStoreStartupStateToSettingsDraft(hwnd);
        }
        break;
    case WM_NCHITTEST: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &pt);
        if (HitTestSettingsTitleControl(pt) != HoverNone) return HTCLIENT;
        if (pt.y >= 0 && pt.y < Ui(kSettingsTitleBarHeight)) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps = {};
        HDC dc = BeginPaint(hwnd, &ps);
        ui_backbuffer::Draw(hwnd, dc, DrawSettingsChrome);
        UpdateHdrPreviewWindow(hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, g_theme.titleText);
        return reinterpret_cast<LRESULT>(g_panelBrush);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, OPAQUE);
        SetTextColor(dc, g_theme.titleText);
        SetBkColor(dc, g_theme.control);
        return reinterpret_cast<LRESULT>(g_editBrush);
    }
    case WM_LBUTTONDOWN: {
        if (ShouldIgnoreSettingsMouse()) {
            g_pressedControl = 0;
            return 0;
        }
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        UpdateSettingsHover(hwnd, pt);
        int titleHover = HitTestSettingsTitleControl(pt);
        if (titleHover != HoverNone) {
            g_pressedControl = titleHover;
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (g_settingsInfoDialogOpen || g_hotkeyDialogOpen) {
            int dialogHover = HitTestSettingsDialogControl(hwnd, pt);
            if (dialogHover == HoverDialogOk || dialogHover == HoverDialogClose ||
                dialogHover == HoverDialogLink || dialogHover == HoverScreenshotHotkey ||
                dialogHover == HoverFullscreenHotkey) {
                g_pressedControl = dialogHover;
                ArmSettingsAnimationTimer(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        int topHover = HitTestSettingsTopControl(pt);
        if (topHover != HoverNone) {
            g_pressedControl = topHover;
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        int footerHover = HitTestSettingsFooterControl(hwnd, pt);
        if (footerHover != HoverNone) {
            g_pressedControl = footerHover;
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        HandleSettingsClick(hwnd, pt);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RememberSettingsMousePoint(pt);
        if (g_draggingBrightnessId != 0 && (wParam & MK_LBUTTON)) {
            POINT contentPt = SettingsContentPoint(pt);
            int dragHover = g_draggingBrightnessId == kIdDayBrightness ? HoverDaySlider : HoverNightSlider;
            if (g_hoverControl != dragHover) {
                g_hoverControl = dragHover;
                ArmSettingsAnimationTimer(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            int value = SetDraftBrightnessFromPoint(hwnd, contentPt, g_draggingBrightnessId);
            ApplySettingsBrightnessPreview(hwnd, g_draggingBrightnessId, value);
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return 0;
        }
        UpdateSettingsHover(hwnd, pt);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (g_settingsInfoDialogOpen || g_hotkeyDialogOpen) return 0;
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta != 0) {
            bool dropdownWasOpen = g_languageDropdownOpen;
            g_languageDropdownOpen = false;
            if (dropdownWasOpen) {
                UpdateHdrPreviewWindow(hwnd);
                ArmSettingsAnimationTimer(hwnd);
            }
            SetSettingsScrollY(hwnd, g_settingsScrollY - MulDiv(delta, 60, WHEEL_DELTA));
        }
        return 0;
    }
    case WM_TIMER:
        if (wParam == kSettingsAnimationTimer) {
            if (UpdateSettingsAnimations(hwnd)) {
                UpdateHdrPreviewWindow(hwnd);
            } else {
                KillTimer(hwnd, kSettingsAnimationTimer);
            }
            return 0;
        }
        break;
    case WM_MOUSELEAVE:
        g_trackingSettingsMouse = false;
        g_settingsMouseKnown = false;
        if (g_pressedControl != 0) {
            g_pressedControl = 0;
            ArmSettingsAnimationTimer(hwnd);
        }
        if (g_hoverControl != HoverNone) {
            g_hoverControl = HoverNone;
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && g_hoverControl != HoverNone) {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_KILLFOCUS:
        if (g_recordingHotkey != 0) {
            g_recordingHotkey = 0;
            RegisterAppHotkeys();
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        if (g_recordingHotkey != 0) {
            UINT vk = static_cast<UINT>(wParam);
            if (vk == VK_ESCAPE) {
                g_recordingHotkey = 0;
                RegisterAppHotkeys();
                ArmSettingsAnimationTimer(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            // 忽略纯修饰键，等待组合键
            if (vk != VK_CONTROL && vk != VK_SHIFT && vk != VK_MENU &&
                vk != VK_LWIN && vk != VK_RWIN) {
                UINT mod = 0;
                if (GetKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
                if (GetKeyState(VK_SHIFT) & 0x8000) mod |= MOD_SHIFT;
                if (GetKeyState(VK_MENU) & 0x8000) mod |= MOD_ALT;
                if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mod |= MOD_WIN;
                if (mod != 0) {
                    ApplyRecordedHotkey(g_recordingHotkey, mod, vk);
                    g_recordingHotkey = 0;
                    SaveConfig();
                    RegisterAppHotkeys();
                    ArmSettingsAnimationTimer(hwnd);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            return 0;
        }
        if (g_settingsInfoDialogOpen && (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_SPACE)) {
            CloseSettingsInfoDialog(hwnd);
            return 0;
        }
        if (g_hotkeyDialogOpen && (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_SPACE)) {
            CloseHotkeyDialog(hwnd);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (ShouldIgnoreSettingsMouse()) {
            g_pressedControl = 0;
            return 0;
        }
        if (g_pressedControl != 0) {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            int pressed = g_pressedControl;
            int released = HoverNone;
            if (IsSettingsTitleControl(pressed)) {
                released = HitTestSettingsTitleControl(pt);
            } else if (pressed >= HoverDialogOk) {
                released = HitTestSettingsDialogControl(hwnd, pt);
            } else if (pressed == HoverSupport) {
                released = HitTestSettingsTopControl(pt);
            } else {
                released = HitTestSettingsFooterControl(hwnd, pt);
            }
            g_pressedControl = 0;
            ArmSettingsAnimationTimer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            if (pressed == released) {
                if (pressed == HoverTitleHelp || pressed == HoverTitleGithub) {
                    OpenGithubRepository(hwnd);
                } else if (pressed == HoverTitleMinimize) {
                    ShowWindow(hwnd, SW_MINIMIZE);
                } else if (pressed == HoverTitleClose) {
                    DestroyWindow(hwnd);
                } else if (pressed == HoverDialogOk || pressed == HoverDialogClose) {
                    if (g_hotkeyDialogOpen) {
                        CloseHotkeyDialog(hwnd);
                    } else {
                        CloseSettingsInfoDialog(hwnd);
                    }
                } else if (pressed == HoverDialogLink) {
                    OpenNightLightSettings(hwnd);
                    CloseSettingsInfoDialog(hwnd);
                } else if (pressed == HoverScreenshotHotkey || pressed == HoverFullscreenHotkey) {
                    StartHotkeyRecording(hwnd, pressed);
                } else if (pressed == HoverOk) {
                    ApplySettingsDraft(hwnd, true);
                } else if (pressed == HoverApply) {
                    ApplySettingsDraft(hwnd, false);
                } else if (pressed == HoverCancel) {
                    RestoreSettingsBrightnessPreviewIfNeeded();
                    DestroyWindow(hwnd);
                } else if (pressed == HoverSupport) {
                    ShowSupportWindow(hwnd);
                }
            }
            return 0;
        }
        if (g_draggingBrightnessId != 0) {
            int draggedBrightnessId = g_draggingBrightnessId;
            bool draggedCurrentMode = SettingsBrightnessIdMatchesCurrentMode(draggedBrightnessId);
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            int value = SetDraftBrightnessFromPoint(hwnd, SettingsContentPoint(pt), draggedBrightnessId);
            ApplySettingsBrightnessPreview(hwnd, draggedBrightnessId, value);
            g_draggingBrightnessId = 0;
            ReleaseCapture();
            if (!draggedCurrentMode) {
                RestoreCurrentModeAfterInactivePreview(hwnd);
            }
            return 0;
        }
        break;
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == kIdOk) {
            ApplySettingsFromWindow(hwnd, true);
            return 0;
        }
        if (id == kIdApply) {
            ApplySettingsFromWindow(hwnd, false);
            return 0;
        }
        if (id == kIdCancel) {
            RestoreSettingsBrightnessPreviewIfNeeded();
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_SETTINGCHANGE:
        RefreshUiDpi(hwnd);
        ReloadUiTheme();
        EnsureUiResources();
        SyncStoreStartupStateToSettingsDraft(hwnd);
        ApplyModernWindowFrame(hwnd);
        UpdateSettingsWindowTitle(hwnd);
        HdrPreviewResetDevice(&g_hdrPreview);
        UpdateHdrPreviewWindow(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_MOVE:
    case WM_DISPLAYCHANGE:
        HdrPreviewUpdateColorSpace(&g_hdrPreview);
        UpdateHdrPreviewWindow(hwnd);
        return 0;
    case WM_CLOSE:
        RestoreSettingsBrightnessPreviewIfNeeded();
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kSettingsAnimationTimer);
        DestroyHdrPreviewWindow();
        RestoreSettingsBrightnessPreviewIfNeeded();
        if (g_recordingHotkey != 0) {
            g_recordingHotkey = 0;
            RegisterAppHotkeys();
        }
        g_settingsWindow = NULL;
        g_settingsDraftActive = false;
        g_draggingBrightnessId = 0;
        g_languageDropdownOpen = false;
        g_settingsInfoDialogOpen = false;
        g_hotkeyDialogOpen = false;
        g_pressedControl = 0;
        g_settingsScrollY = 0;
        g_hoverControl = HoverNone;
        g_settingsWindowAnim = 0;
        g_settingsDialogAnim = 0;
        g_settingsDropdownAnim = 0;
        for (int i = 0; i < kAnimationSlotCount; ++i) {
            g_controlAnim[i] = 0;
        }
        g_trackingSettingsMouse = false;
        g_settingsMouseKnown = false;
        CleanupUiResources();
        ShutdownGdiplus();
        UnregisterClassW(L"HdrSdrBrightnessSettingsWindow", g_instance);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ShowSettingsWindow(HWND owner) {
    if (g_settingsWindow) {
        SyncStoreStartupStateToSettingsDraft(g_settingsWindow);
        UpdateSettingsWindowTitle(g_settingsWindow);
        ShowWindow(g_settingsWindow, SW_SHOWNORMAL);
        ApplyModernWindowFrame(g_settingsWindow);
        InvalidateRect(g_settingsWindow, NULL, TRUE);
        SetForegroundWindow(g_settingsWindow);
        return;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = g_instance;
    wc.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON));
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RefreshUiDpiForNewTopLevelWindow(owner);
    EnsureUiResources();
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"HdrSdrBrightnessSettingsWindow";
    RegisterClassExW(&wc);

    DWORD style = WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
    DWORD exStyle = 0;
    LoadConfig(true);
    Config previewConfig = g_config;
    bool canFollow = CanFollowWindowsNightLight();
    if (!canFollow) {
        previewConfig.followNightLight = false;
    }
    SettingsLayout layout = BuildSettingsLayout(previewConfig.followNightLight && canFollow);
    int visibleHeight = SettingsVisibleClientHeight(layout.clientHeight, style, exStyle);
    SIZE windowSize = WindowSizeForClient(style, exStyle, Ui(kSettingsClientWidth),
                                          Ui(visibleHeight + kSettingsTitleBarHeight));

    g_settingsWindow = CreateWindowExW(exStyle, wc.lpszClassName, T(TxtSettingsTitle),
                                       style,
                                       CW_USEDEFAULT, CW_USEDEFAULT, windowSize.cx, windowSize.cy,
                                       owner, NULL, g_instance, NULL);
    if (g_settingsWindow) {
        RefreshUiDpi(g_settingsWindow);
        layout = BuildSettingsLayout(previewConfig.followNightLight && canFollow);
        visibleHeight = SettingsVisibleClientHeight(layout.clientHeight, style, exStyle);
        windowSize = WindowSizeForClient(style, exStyle, Ui(kSettingsClientWidth),
                                         Ui(visibleHeight + kSettingsTitleBarHeight));
    }
    CenterWindow(g_settingsWindow, windowSize.cx, windowSize.cy);
    ShowWindow(g_settingsWindow, SW_SHOW);
    UpdateWindow(g_settingsWindow);
}

void StartRegistryThread(HWND notifyWindow) {
    registry_watcher::Start(&g_registryWatcher, notifyWindow, kRegistryChangedMessage, kConfigKey);
}

void StopRegistryThread() {
    registry_watcher::Stop(&g_registryWatcher);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == g_taskbarCreated) {
        AddTrayIcon(hwnd);
        UpdateTrayTip();
        return 0;
    }

    switch (message) {
    case WM_CREATE:
        AddTrayIcon(hwnd);
        StartRegistryThread(hwnd);
        SetTimer(hwnd, kRecheckTimer, kRecheckMs, NULL);
        SetTimer(hwnd, kCaptureWarmupTimer, kCaptureWarmupMs, NULL);
        PostMessageW(hwnd, kApplyMessage, TRUE, 0);
        return 0;
    case kApplyMessage:
        ApplyCurrentBrightness(wParam != 0);
        CheckWeeklySupportReminder();
        return 0;
    case kRegistryChangedMessage:
        InvalidateNightLightScheduleCache();
        PostMessageW(hwnd, kApplyMessage, FALSE, 0);
        return 0;
    case WM_TIMER:
        if (wParam == kRecheckTimer) {
            night_mode::InvalidateActiveStateCache();
            ApplyCurrentBrightness(false);
            CheckWeeklySupportReminder();
            return 0;
        }
        if (wParam == kTransitionTimer) {
            ContinueBrightnessTransition();
            return 0;
        }
        if (wParam == kCaptureWarmupTimer) {
            KillTimer(hwnd, kCaptureWarmupTimer);
            StartNativeCaptureHelperServer();
            StartNativeEditorWarmup();
            return 0;
        }
        break;
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
        if (message == WM_SETTINGCHANGE) {
            ReloadUiTheme();
            if (g_settingsWindow) {
                RefreshUiDpi(g_settingsWindow);
                EnsureUiResources();
                ApplyModernWindowFrame(g_settingsWindow);
                InvalidateRect(g_settingsWindow, NULL, TRUE);
            }
        }
        InvalidateNightLightScheduleCache();
        PostMessageW(hwnd, kApplyMessage, TRUE, 0);
        return 0;
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            PostMessageW(hwnd, kApplyMessage, TRUE, 0);
        }
        return TRUE;
    case WM_HOTKEY:
        if (wParam == kHotkeyIdScreenshot) {
            LaunchHdrScreenshotHelper(hwnd);
            return 0;
        }
        if (wParam == kHotkeyIdFullscreen) {
            LaunchHdrFullscreenCapture(hwnd);
            return 0;
        }
        break;
    case kRegionCaptureDoneMessage: {
        auto* result = reinterpret_cast<RegionCaptureResult*>(lParam);
        if (!result) return 0;
        capture_request::CompletionDecision completion =
            g_regionCaptureRequests.Complete(result->generation);
        bool captured = result->captured;
        bool helperMissing = result->helperMissing;
        std::wstring editCommand = result->editCommand;
        delete result;

        if (completion.action == capture_request::CompletionAction::StartLatest) {
            StartRegionCapture(hwnd, completion.generation);
            return 0;
        }
        if (completion.action != capture_request::CompletionAction::ShowResult) {
            return 0;
        }
        if (!captured) {
            ShowTrayNotification(T(TxtMenuHdrScreenshot),
                                 T(helperMissing ? TxtCaptureHelperMissing
                                                 : TxtCaptureLaunchFailed),
                                 NotificationActionDefault);
            return 0;
        }

        editor_window_control::CloseAll();
        if (!LaunchDetached(editCommand,
                            DirectoryFromPath(fullscreen_capture::GetEditorHelperPath()))) {
            ShowTrayNotification(T(TxtMenuHdrScreenshot), T(TxtCaptureLaunchFailed),
                                 NotificationActionDefault);
        }
        return 0;
    }
    case kFullscreenDoneMessage:
        if (wParam) {
            ShowTrayNotification(T(TxtMenuHdrScreenshot), T(TxtHotkeyCopied),
                                 NotificationActionFullscreenScreenshot);
        } else {
            g_lastFullscreenCapturePath.clear();
            ShowTrayNotification(T(TxtMenuHdrScreenshot), T(TxtCaptureLaunchFailed),
                                 NotificationActionDefault);
        }
        return 0;
    case kStoreLicenseExpiredMessage:
        DisableStoreStartupAfterLicenseExpired();
        DestroyWindow(hwnd);
        return 0;
    case kTrayMessage:
        if (LOWORD(lParam) == NIN_BALLOONUSERCLICK) {
            ShowLastNotificationDialog();
            return 0;
        }
        if (LOWORD(lParam) == WM_CONTEXTMENU || LOWORD(lParam) == WM_RBUTTONUP) {
            ShowTrayMenu(hwnd);
            return 0;
        }
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            ShowSettingsWindow(hwnd);
            return 0;
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kMenuApply:
            PostMessageW(hwnd, kApplyMessage, TRUE, 0);
            return 0;
        case kMenuSettings:
            ShowSettingsWindow(hwnd);
            return 0;
        case kMenuStartup:
            if (!IsStartupFeatureAvailable()) return 0;
            g_config.startWithWindows = !IsStartupEnabled();
            SaveConfig();
            return 0;
        case kMenuDisplaySettings:
            ShellExecuteW(hwnd, L"open", L"ms-settings:display", NULL, NULL, SW_SHOWNORMAL);
            return 0;
        case kMenuNightLightSettings:
            ShellExecuteW(hwnd, L"open", L"ms-settings:nightlight", NULL, NULL, SW_SHOWNORMAL);
            return 0;
        case kMenuHdrCalibration:
            OpenHdrCalibration(hwnd);
            return 0;
        case kMenuHdrScreenshot:
            LaunchHdrScreenshotHelper(hwnd);
            return 0;
        case kMenuSupport:
            if (!IsSupportFeatureAvailable()) return 0;
            ShowSupportWindow(hwnd);
            return 0;
        case kMenuExit:
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_DESTROY:
        if (g_supportWindow) DestroyWindow(g_supportWindow);
        KillTimer(hwnd, kRecheckTimer);
        KillTimer(hwnd, kCaptureWarmupTimer);
        StopRegistryThread();
        RemoveTrayIcon();
        CleanupUiResources();
        ShutdownGdiplus();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool CreateMainWindow() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = g_instance;
    wc.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON));
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"HdrSdrBrightnessMainWindow";

    if (!RegisterClassExW(&wc)) return false;
    g_mainWindow = CreateWindowExW(0, wc.lpszClassName, T(TxtDisplayName), WS_OVERLAPPED,
                                   0, 0, 0, 0, NULL, NULL, g_instance, NULL);
    return g_mainWindow != NULL;
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    ui_dpi::EnablePerMonitorV2();
    g_instance = instance;
    bool openSettingsOnLaunch = launch_mode::ShouldOpenSettingsOnLaunch();
    bool backgroundLaunch = !openSettingsOnLaunch;
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    LoadConfig(false);

    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\OledHdrSdrSyncMutex");
    if (!mutex) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (openSettingsOnLaunch) {
            launch_mode::ShowSettingsInExistingInstance(kMenuSettings);
        }
        CloseHandle(mutex);
        return 0;
    }

    if (!CreateMainWindow()) {
        CloseHandle(mutex);
        return 1;
    }
    RegisterAppHotkeys();
    StartStartupRepairThread();
    if (backgroundLaunch) {
        StartStoreLicenseCheckThread(g_mainWindow);
    }

    if (openSettingsOnLaunch) {
        PostMessageW(g_mainWindow, WM_COMMAND, MAKEWPARAM(kMenuSettings, 0), 0);
    }

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        // 录制快捷键时不让 IsDialogMessageW 吞掉按键消息
        bool skipDialog = g_recordingHotkey != 0 &&
                          (msg.message == WM_KEYDOWN || msg.message == WM_KEYUP ||
                           msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP);
        if (g_settingsWindow && !skipDialog && IsDialogMessageW(g_settingsWindow, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(mutex);
    ShutdownGdiplus();
    return 0;
}
