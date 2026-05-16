#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#define _WIN32_IE 0x0600

#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <objbase.h>
#include <shellapi.h>

#ifndef CLEARTYPE_NATURAL_QUALITY
#define CLEARTYPE_NATURAL_QUALITY 6
#endif

#include <algorithm>
#include <cwchar>
#include <sstream>
#include <string>
#include <vector>

namespace {

const wchar_t kAppName[] = L"HdrSdrBrightness";
const wchar_t kLegacySyncAppName[] = L"HdrSdrSync";
const wchar_t kLegacyOledAppName[] = L"OledHdrSdrSync";
const wchar_t kDisplayName[] = L"HDR SDR Brightness";
const wchar_t kAppUserModelId[] = L"HdrSdrBrightness.Desktop";
const wchar_t kConfigKey[] = L"Software\\OledHdrSdrSync";
const wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const int IDI_APPICON = 101;

const UINT kTrayMessage = WM_APP + 1;
const UINT kApplyMessage = WM_APP + 2;
const UINT kRegistryChangedMessage = WM_APP + 3;
const UINT_PTR kRecheckTimer = 1;
const UINT_PTR kTransitionTimer = 2;
const UINT kRecheckMs = 15 * 1000;
const UINT kTransitionMs = 45;
const UINT32 kTransitionStepLevel = 50;
const int kSettingsClientWidth = 640;
const int kSettingsClientHeight = 620;

const UINT kMenuApply = 1001;
const UINT kMenuSettings = 1002;
const UINT kMenuStartup = 1003;
const UINT kMenuDisplaySettings = 1004;
const UINT kMenuNightLightSettings = 1005;
const UINT kMenuExit = 1006;

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

#ifndef NIN_SELECT
#define NIN_SELECT (WM_USER + 0)
#endif
#ifndef NIN_BALLOONUSERCLICK
#define NIN_BALLOONUSERCLICK (WM_USER + 5)
#endif
#ifndef NIF_SHOWTIP
#define NIF_SHOWTIP 0x00000080
#endif

enum LanguageChoice {
    LangAuto = 0,
    LangEnglish = 1,
    LangChinese = 2
};

enum HoverControl {
    HoverNone = 0,
    HoverLanguageBase = 100,
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
    HoverCancel = 603
};

enum TextId {
    TxtStarting,
    TxtDisplayName,
    TxtMenuApply,
    TxtMenuSettings,
    TxtMenuStartup,
    TxtMenuDisplaySettings,
    TxtMenuNightLightSettings,
    TxtMenuExit,
    TxtSettingsTitle,
    TxtSettingsSubtitle,
    TxtLanguage,
    TxtLanguageAuto,
    TxtLanguageEnglish,
    TxtLanguageChinese,
    TxtBrightnessGroup,
    TxtDay,
    TxtNight,
    TxtBehaviorGroup,
    TxtFollowNightLight,
    TxtAutoRestoreManual,
    TxtStartWithWindows,
    TxtFallbackSchedule,
    TxtSwitchMode,
    TxtFollowSystem,
    TxtUseDefaultSchedule,
    TxtNightLightUnavailable,
    TxtFollowSystemHint,
    TxtNightStarts,
    TxtDayStarts,
    TxtOk,
    TxtApply,
    TxtCancel,
    TxtInvalidDayBrightness,
    TxtInvalidNightBrightness,
    TxtInvalidSchedule,
    TxtSourceNightLight,
    TxtSourceFixed,
    TxtAppliedDwm,
    TxtAppliedToDisplays,
    TxtNoHdrDisplay,
    TxtApplyFailed,
    TxtVia,
    TxtNotifyTitle,
    TxtNotifyBody,
    TxtNotifyDialogTitle,
    TxtManualRestoreOff,
    TxtTextCount
};

const wchar_t* const kTextEn[TxtTextCount] = {
    L"Starting",
    L"HDR SDR Brightness",
    L"Apply now",
    L"Settings...",
    L"Start with Windows",
    L"Open Display settings",
    L"Open Night Light settings",
    L"Exit",
    L"HDR SDR Brightness Settings",
    L"Adjust SDR content brightness automatically while HDR is enabled.",
    L"Language",
    L"Auto",
    L"English",
    L"Chinese",
    L"SDR content brightness",
    L"Day",
    L"Night",
    L"Behavior",
    L"Follow Windows Night Light",
    L"Auto restore manual brightness changes",
    L"Start with Windows",
    L"Fallback schedule",
    L"Switching",
    L"Follow system",
    L"Default schedule",
    L"Windows Night Light is not enabled or cannot be read. Use the default schedule instead.",
    L"SDR content brightness follows Windows Night Light.",
    L"Night starts",
    L"Day starts",
    L"OK",
    L"Apply",
    L"Cancel",
    L"Day SDR content brightness must be 0-100.",
    L"Night SDR content brightness must be 0-100.",
    L"Schedule time is invalid.",
    L"Windows Night Light",
    L"Fixed",
    L"Applied with DWM fallback",
    L"Applied to",
    L"No active HDR display found",
    L"Apply failed, error",
    L"via",
    L"SDR content brightness restored",
    L"Auto restore is enabled, so SDR content brightness is being restored to",
    L"SDR content brightness notification",
    L"Manual SDR content brightness changed; auto restore is off"
};

const wchar_t* const kTextZh[TxtTextCount] = {
    L"启动中",
    L"HDR SDR 亮度助手",
    L"立即应用",
    L"设置...",
    L"开机自启",
    L"打开显示设置",
    L"打开夜间模式设置",
    L"退出",
    L"HDR SDR 亮度助手设置",
    L"在开启 HDR 时自动调整 SDR 内容亮度。",
    L"语言",
    L"自动",
    L"English",
    L"中文",
    L"SDR 内容亮度",
    L"白天",
    L"夜间",
    L"行为",
    L"跟随 Windows 夜间模式",
    L"自动纠正手动调整",
    L"开机自启",
    L"默认时段",
    L"切换方式",
    L"跟随系统",
    L"默认时段",
    L"未检测到可跟随的 Windows 夜间模式，请使用默认时段。",
    L"SDR 内容亮度将跟随 Windows 夜间模式自动切换。",
    L"夜间模式开始",
    L"白天模式开始",
    L"确定",
    L"应用",
    L"取消",
    L"白天 SDR 内容亮度必须为 0-100。",
    L"夜间 SDR 内容亮度必须为 0-100。",
    L"时段设置无效。",
    L"Windows 夜间模式",
    L"固定时段",
    L"已使用 DWM 兜底方式应用",
    L"已应用到",
    L"未找到已启用 HDR 的显示器",
    L"应用失败，错误",
    L"跟随",
    L"正在恢复 SDR 内容亮度",
    L"已开启自动纠正手动调整，正在将 SDR 内容亮度恢复到",
    L"SDR 内容亮度通知",
    L"检测到手动修改 SDR 内容亮度；自动恢复已关闭"
};

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
typedef LONG(WINAPI* QueryDisplayConfigFn)(UINT32, UINT32*, AppDisplayConfigPathInfo*, UINT32*, AppDisplayConfigModeInfo*, UINT32*);
typedef LONG(WINAPI* DisplayConfigGetDeviceInfoFn)(AppDisplayConfigDeviceInfoHeader*);
typedef LONG(WINAPI* DisplayConfigSetDeviceInfoFn)(AppDisplayConfigDeviceInfoHeader*);
typedef HRESULT(WINAPI* DwmpSdrToHdrBoostFn)(HMONITOR, double);

struct DisplayConfigApi {
    GetDisplayConfigBufferSizesFn getBufferSizes;
    QueryDisplayConfigFn query;
    DisplayConfigGetDeviceInfoFn getDeviceInfo;
    DisplayConfigSetDeviceInfoFn setDeviceInfo;
};

struct Config {
    int dayBrightness;
    int nightBrightness;
    bool followNightLight;
    bool autoRestoreManualChanges;
    bool startWithWindows;
    int language;
    int nightStartHour;
    int nightStartMinute;
    int dayStartHour;
    int dayStartMinute;

    Config()
        : dayBrightness(25),
          nightBrightness(10),
          followNightLight(true),
          autoRestoreManualChanges(true),
          startWithWindows(false),
          language(LangAuto),
          nightStartHour(18),
          nightStartMinute(0),
          dayStartHour(8),
          dayStartMinute(0) {}
};

struct MaybeBool {
    bool known;
    bool value;

    MaybeBool() : known(false), value(false) {}
    MaybeBool(bool knownValue, bool boolValue) : known(knownValue), value(boolValue) {}
};

struct NightDecision {
    bool night;
    std::wstring source;
};

struct ApplyResult {
    bool ok;
    int targetCount;
    int successCount;
    LONG lastError;
    bool usedDwmFallback;
    bool changed;
    bool complete;

    ApplyResult()
        : ok(false),
          targetCount(0),
          successCount(0),
          lastError(ERROR_SUCCESS),
          usedDwmFallback(false),
          changed(false),
          complete(false) {}
};

HINSTANCE g_instance = NULL;
HWND g_mainWindow = NULL;
HWND g_settingsWindow = NULL;
NOTIFYICONDATAW g_tray = {};
UINT g_taskbarCreated = 0;
Config g_config;
Config g_settingsDraft;
bool g_settingsDraftActive = false;
std::wstring g_status = L"Starting";
std::wstring g_lastNotificationTitle;
std::wstring g_lastNotificationBody;
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
int g_hoverControl = 0;
bool g_trackingSettingsMouse = false;
HANDLE g_stopEvent = NULL;
HANDLE g_registryThread = NULL;
MaybeBool g_cachedSunsetSchedule;
bool g_sunsetScheduleCacheValid = false;
HFONT g_uiFont = NULL;
HFONT g_smallFont = NULL;
HFONT g_titleFont = NULL;
HFONT g_sectionFont = NULL;
HFONT g_heroFont = NULL;
HBRUSH g_windowBrush = NULL;
HBRUSH g_panelBrush = NULL;
HBRUSH g_editBrush = NULL;
ULONG_PTR g_gdiplusToken = 0;

struct UiTheme {
    bool dark;
    COLORREF window;
    COLORREF card;
    COLORREF cardBorder;
    COLORREF cardHover;
    COLORREF elevated;
    COLORREF control;
    COLORREF controlHover;
    COLORREF controlBorder;
    COLORREF controlBorderHover;
    COLORREF text;
    COLORREF mutedText;
    COLORREF titleText;
    COLORREF track;
    COLORREF trackHover;
    COLORREF primary;
    COLORREF primaryHover;
    COLORREF primaryBorderHover;
    COLORREF knob;
    COLORREF disabledText;
};

UiTheme g_theme = {};

static_assert(sizeof(AppDisplayConfigPathSourceInfo) == 20, "Unexpected DISPLAYCONFIG source size");
static_assert(sizeof(AppDisplayConfigPathTargetInfo) == 48, "Unexpected DISPLAYCONFIG target size");
static_assert(sizeof(AppDisplayConfigPathInfo) == 72, "Unexpected DISPLAYCONFIG path size");
static_assert(sizeof(AppDisplayConfigModeInfo) == 64, "Unexpected DISPLAYCONFIG mode size");
static_assert(sizeof(AppDisplayConfigDeviceInfoHeader) == 20, "Unexpected DISPLAYCONFIG header size");
static_assert(sizeof(AppDisplayConfigGetAdvancedColorInfo) == 32, "Unexpected advanced color size");
static_assert(sizeof(AppDisplayConfigSdrWhiteLevel) == 24, "Unexpected SDR white level size");

int ClampInt(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

void CopyString(wchar_t* buffer, size_t count, const std::wstring& value) {
    if (!buffer || count == 0) return;
    std::wcsncpy(buffer, value.c_str(), count - 1);
    buffer[count - 1] = L'\0';
}

void SetProcessAppUserModelId() {
    HMODULE shell = LoadLibraryW(L"shell32.dll");
    if (!shell) return;

    typedef HRESULT(WINAPI* SetCurrentProcessExplicitAppUserModelIDFn)(PCWSTR);
    SetCurrentProcessExplicitAppUserModelIDFn setAppId =
        reinterpret_cast<SetCurrentProcessExplicitAppUserModelIDFn>(
            GetProcAddress(shell, "SetCurrentProcessExplicitAppUserModelID"));
    if (setAppId) {
        setAppId(kAppUserModelId);
    }

    FreeLibrary(shell);
}

bool ShouldUseChineseUi() {
    int language = g_settingsDraftActive ? g_settingsDraft.language : g_config.language;
    if (language == LangChinese) return true;
    if (language == LangEnglish) return false;

    LANGID langId = GetUserDefaultUILanguage();
    return PRIMARYLANGID(langId) == LANG_CHINESE;
}

const wchar_t* T(TextId id) {
    if (id < 0 || id >= TxtTextCount) return L"";
    return ShouldUseChineseUi() ? kTextZh[id] : kTextEn[id];
}

std::wstring PercentLabel(int value);

std::wstring GetExePath() {
    std::vector<wchar_t> path(MAX_PATH);
    DWORD length = 0;
    for (;;) {
        length = GetModuleFileNameW(NULL, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) return L"";
        if (length < path.size() - 1) break;
        path.resize(path.size() * 2);
    }
    return std::wstring(path.data(), length);
}

std::wstring QuotePath(const std::wstring& path) {
    return L"\"" + path + L"\"";
}

bool IsBackgroundLaunchArgument(const wchar_t* arg) {
    if (!arg) return false;
    return lstrcmpiW(arg, L"--background") == 0 ||
           lstrcmpiW(arg, L"/background") == 0 ||
           lstrcmpiW(arg, L"--tray") == 0 ||
           lstrcmpiW(arg, L"/tray") == 0;
}

bool ShouldOpenSettingsOnLaunch() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return true;

    bool background = false;
    for (int i = 1; i < argc; ++i) {
        if (IsBackgroundLaunchArgument(argv[i])) {
            background = true;
            break;
        }
    }

    LocalFree(argv);
    return !background;
}

void ShowSettingsInExistingInstance() {
    HWND hwnd = FindWindowW(L"HdrSdrBrightnessMainWindow", NULL);
    if (!hwnd) {
        hwnd = FindWindowW(L"HdrSdrSyncMainWindow", NULL);
    }
    if (!hwnd) {
        hwnd = FindWindowW(L"OledHdrSdrSyncMainWindow", NULL);
    }
    if (hwnd) {
        PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(kMenuSettings, 0), 0);
    }
}

bool ReadDwordValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, DWORD* value) {
    HKEY key = NULL;
    LONG rc = RegOpenKeyExW(root, keyPath, 0, KEY_READ, &key);
    if (rc != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    DWORD data = 0;
    rc = RegQueryValueExW(key, valueName, NULL, &type, reinterpret_cast<LPBYTE>(&data), &size);
    RegCloseKey(key);

    if (rc != ERROR_SUCCESS || type != REG_DWORD || size != sizeof(DWORD)) return false;
    *value = data;
    return true;
}

void WriteDwordValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, DWORD value) {
    HKEY key = NULL;
    LONG rc = RegCreateKeyExW(root, keyPath, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
    if (rc != ERROR_SUCCESS) return;
    RegSetValueExW(key, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
}

bool ReadBinaryValue(HKEY root, const wchar_t* keyPath, const wchar_t* valueName, std::vector<BYTE>* data) {
    HKEY key = NULL;
    LONG rc = RegOpenKeyExW(root, keyPath, 0, KEY_READ, &key);
    if (rc != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD size = 0;
    rc = RegQueryValueExW(key, valueName, NULL, &type, NULL, &size);
    if (rc != ERROR_SUCCESS || type != REG_BINARY || size == 0) {
        RegCloseKey(key);
        return false;
    }

    std::vector<BYTE> buffer(size);
    rc = RegQueryValueExW(key, valueName, NULL, &type, buffer.data(), &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || type != REG_BINARY) return false;

    buffer.resize(size);
    data->swap(buffer);
    return true;
}

bool IsStartupEnabled() {
    HKEY key = NULL;
    LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key);
    if (rc != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD size = 0;
    rc = RegQueryValueExW(key, kAppName, NULL, &type, NULL, &size);
    if (rc == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && size > 0) {
        RegCloseKey(key);
        return true;
    }

    type = 0;
    size = 0;
    rc = RegQueryValueExW(key, kLegacySyncAppName, NULL, &type, NULL, &size);
    if (rc == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && size > 0) {
        RegCloseKey(key);
        return true;
    }

    type = 0;
    size = 0;
    rc = RegQueryValueExW(key, kLegacyOledAppName, NULL, &type, NULL, &size);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && size > 0;
}

bool FileExists(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool IsWow64ProcessCurrent() {
    typedef BOOL(WINAPI* IsWow64ProcessFn)(HANDLE, PBOOL);
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) return false;

    IsWow64ProcessFn fn = reinterpret_cast<IsWow64ProcessFn>(GetProcAddress(kernel32, "IsWow64Process"));
    if (!fn) return false;

    BOOL isWow64 = FALSE;
    if (!fn(GetCurrentProcess(), &isWow64)) return false;
    return isWow64 != FALSE;
}

std::wstring GetWindowsDirectoryPath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    UINT length = GetWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    if (length == 0) return L"C:\\Windows";
    if (length >= buffer.size()) {
        buffer.resize(length + 1);
        length = GetWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    }
    return std::wstring(buffer.data(), length);
}

std::wstring GetCloudSettingsReaderPath() {
    std::wstring windows = GetWindowsDirectoryPath();
    std::wstring sysnative = windows + L"\\Sysnative\\readCloudDataSettings.exe";
    if (IsWow64ProcessCurrent() && FileExists(sysnative)) return sysnative;

    std::wstring system32 = windows + L"\\System32\\readCloudDataSettings.exe";
    if (FileExists(system32)) return system32;

    return L"";
}

bool ReadPipeAvailable(HANDLE pipe, std::string* output) {
    DWORD available = 0;
    if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL)) return false;
    if (available == 0) return true;

    std::vector<char> buffer(available);
    DWORD read = 0;
    if (!ReadFile(pipe, buffer.data(), available, &read, NULL)) return false;
    output->append(buffer.data(), buffer.data() + read);
    return true;
}

bool RunProcessCapture(const std::wstring& commandLine, DWORD timeoutMs, std::string* output) {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = NULL;
    HANDLE writePipe = NULL;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return false;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    BOOL created = CreateProcessW(NULL, mutableCommand.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW,
                                  NULL, NULL, &si, &pi);
    CloseHandle(writePipe);
    if (!created) {
        CloseHandle(readPipe);
        return false;
    }

    DWORD startTick = GetTickCount();
    bool timedOut = false;
    for (;;) {
        ReadPipeAvailable(readPipe, output);
        DWORD wait = WaitForSingleObject(pi.hProcess, 25);
        if (wait == WAIT_OBJECT_0) break;
        if (GetTickCount() - startTick > timeoutMs) {
            timedOut = true;
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 500);
            break;
        }
    }
    ReadPipeAvailable(readPipe, output);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(readPipe);

    return !timedOut && exitCode == 0;
}

bool ReadCloudDataSetting(const wchar_t* typeName, std::string* output) {
    std::wstring reader = GetCloudSettingsReaderPath();
    if (reader.empty()) return false;

    std::wstring command = QuotePath(reader) + L" get -type:" + typeName;
    output->clear();
    return RunProcessCapture(command, 1500, output);
}

bool ContainsText(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
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

void SetStartupEnabled(bool enabled) {
    HKEY key = NULL;
    LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
    if (rc != ERROR_SUCCESS) return;

    if (enabled) {
        std::wstring command = QuotePath(GetExePath()) + L" --background";
        RegSetValueExW(key, kAppName, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                       static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        RegDeleteValueW(key, kLegacySyncAppName);
        RegDeleteValueW(key, kLegacyOledAppName);
    } else {
        RegDeleteValueW(key, kAppName);
        RegDeleteValueW(key, kLegacySyncAppName);
        RegDeleteValueW(key, kLegacyOledAppName);
    }

    RegCloseKey(key);
}

void LoadConfig() {
    DWORD value = 0;
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"DayBrightness", &value)) {
        g_config.dayBrightness = ClampInt(static_cast<int>(value), 0, 100);
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"NightBrightness", &value)) {
        g_config.nightBrightness = ClampInt(static_cast<int>(value), 0, 100);
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"FollowNightLight", &value)) {
        g_config.followNightLight = value != 0;
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"AutoRestoreManualChanges", &value)) {
        g_config.autoRestoreManualChanges = value != 0;
    }
    if (ReadDwordValue(HKEY_CURRENT_USER, kConfigKey, L"Language", &value)) {
        g_config.language = ClampInt(static_cast<int>(value), LangAuto, LangChinese);
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
    g_config.startWithWindows = IsStartupEnabled();
    if (g_status == L"Starting" || g_status == L"启动中") {
        g_status = T(TxtStarting);
    }
}

void SaveConfig() {
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"DayBrightness", g_config.dayBrightness);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"NightBrightness", g_config.nightBrightness);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"FollowNightLight", g_config.followNightLight ? 1 : 0);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"AutoRestoreManualChanges", g_config.autoRestoreManualChanges ? 1 : 0);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"Language", static_cast<DWORD>(g_config.language));
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"NightStartHour", g_config.nightStartHour);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"NightStartMinute", g_config.nightStartMinute);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"DayStartHour", g_config.dayStartHour);
    WriteDwordValue(HKEY_CURRENT_USER, kConfigKey, L"DayStartMinute", g_config.dayStartMinute);
    SetStartupEnabled(g_config.startWithWindows);
}

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

void InvalidateNightLightScheduleCache() {
    g_sunsetScheduleCacheValid = false;
}

bool CanFollowWindowsNightLight() {
    MaybeBool schedule = GetNightLightSunsetScheduleCached();
    return schedule.known && schedule.value;
}

bool IsFixedNightNow() {
    SYSTEMTIME local = {};
    GetLocalTime(&local);
    int now = local.wHour * 60 + local.wMinute;
    int nightStart = g_config.nightStartHour * 60 + g_config.nightStartMinute;
    int dayStart = g_config.dayStartHour * 60 + g_config.dayStartMinute;

    if (nightStart == dayStart) return false;
    if (nightStart < dayStart) {
        return now >= nightStart && now < dayStart;
    }
    return now >= nightStart || now < dayStart;
}

std::wstring FormatTwoDigit(int value) {
    std::wstringstream ss;
    if (value < 10) ss << L"0";
    ss << value;
    return ss.str();
}

NightDecision DecideNight() {
    if (g_config.followNightLight) {
        MaybeBool sunsetSchedule = GetNightLightSunsetScheduleCached();
        if (sunsetSchedule.known && sunsetSchedule.value) {
            MaybeBool nightLight = ReadNightLightActive();
            if (!nightLight.known) {
                nightLight = ReadNightLightActiveViaCloudReader();
            }
            if (nightLight.known) {
                NightDecision decision;
                decision.night = nightLight.value;
                decision.source = T(TxtSourceNightLight);
                return decision;
            }
        } else if (!sunsetSchedule.known) {
            MaybeBool manualSchedule = NightLightLooksLikeManualSchedule();
            if (!manualSchedule.known || !manualSchedule.value) {
                MaybeBool nightLight = ReadNightLightActive();
                if (nightLight.known) {
                    NightDecision decision;
                    decision.night = nightLight.value;
                    decision.source = T(TxtSourceNightLight);
                    return decision;
                }
            }
        }

    }

    NightDecision decision;
    decision.night = IsFixedNightNow();
    std::wstringstream ss;
    ss << T(TxtSourceFixed) << L" "
       << FormatTwoDigit(g_config.nightStartHour) << L":" << FormatTwoDigit(g_config.nightStartMinute)
       << L"-"
       << FormatTwoDigit(g_config.dayStartHour) << L":" << FormatTwoDigit(g_config.dayStartMinute);
    decision.source = ss.str();
    return decision;
}

bool LoadDisplayConfigApi(DisplayConfigApi* api) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) user32 = LoadLibraryW(L"user32.dll");
    if (!user32) return false;

    api->getBufferSizes = reinterpret_cast<GetDisplayConfigBufferSizesFn>(GetProcAddress(user32, "GetDisplayConfigBufferSizes"));
    api->query = reinterpret_cast<QueryDisplayConfigFn>(GetProcAddress(user32, "QueryDisplayConfig"));
    api->getDeviceInfo = reinterpret_cast<DisplayConfigGetDeviceInfoFn>(GetProcAddress(user32, "DisplayConfigGetDeviceInfo"));
    api->setDeviceInfo = reinterpret_cast<DisplayConfigSetDeviceInfoFn>(GetProcAddress(user32, "DisplayConfigSetDeviceInfo"));

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
        rc = api->query(kQdcOnlyActivePaths, &queryPathCount, pathBuffer.data(), &queryModeCount, modeBuffer.data(), NULL);
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

struct DwmFallbackContext {
    DwmpSdrToHdrBoostFn fn;
    double boost;
    int successCount;
};

BOOL CALLBACK ApplyDwmFallbackToMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM param) {
    DwmFallbackContext* context = reinterpret_cast<DwmFallbackContext*>(param);
    HRESULT hr = context->fn(monitor, context->boost);
    if (SUCCEEDED(hr)) ++context->successCount;
    return TRUE;
}

bool ApplyDwmFallback(UINT32 sdrLevel, int* successCount) {
    HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
    if (!dwmapi) return false;

    DwmpSdrToHdrBoostFn fn = reinterpret_cast<DwmpSdrToHdrBoostFn>(GetProcAddress(dwmapi, MAKEINTRESOURCEA(171)));
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

UINT32 BrightnessPercentToSdrLevel(int brightness) {
    brightness = ClampInt(brightness, 0, 100);
    return 1000u + static_cast<UINT32>(brightness) * 50u;
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

ApplyResult ApplySdrLevelStep(UINT32 targetLevel, bool smooth) {
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
                    nextLevel = MoveLevelToward(currentLevel, targetLevel, kTransitionStepLevel);
                }

                LONG rc = SetSdrWhiteLevel(&api, path, nextLevel);
                result.lastError = rc;
                if (rc == ERROR_SUCCESS) {
                    ++result.successCount;
                    result.changed = true;
                }
            }
        }
    }

    result.ok = result.targetCount > 0 && result.successCount == result.targetCount;
    if (!result.ok) {
        int dwmSuccess = 0;
        if (ApplyDwmFallback(targetLevel, &dwmSuccess)) {
            result.ok = true;
            result.usedDwmFallback = true;
            result.successCount += dwmSuccess;
            result.changed = true;
            result.complete = true;
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

std::wstring BuildStatusText(const NightDecision& decision, int brightness, const ApplyResult& result) {
    std::wstringstream ss;
    if (ShouldUseChineseUi()) {
        ss << L"SDR 内容亮度 " << (decision.night ? T(TxtNight) : T(TxtDay)) << L" " << PercentLabel(brightness);
    } else {
        ss << L"SDR content brightness " << (decision.night ? T(TxtNight) : T(TxtDay)) << L" " << PercentLabel(brightness);
    }
    ss << L" " << T(TxtVia) << L" " << decision.source << L". ";

    if (result.ok) {
        if (result.usedDwmFallback) {
            ss << T(TxtAppliedDwm);
        } else {
            ss << T(TxtAppliedToDisplays) << L" " << result.successCount << L"/" << result.targetCount << L" HDR";
        }
    } else if (result.targetCount == 0) {
        ss << T(TxtNoHdrDisplay);
    } else {
        ss << T(TxtApplyFailed) << L" " << result.lastError;
    }

    return ss.str();
}

void UpdateTrayTip() {
    if (!g_mainWindow) return;
    std::wstringstream tip;
    tip << T(TxtDisplayName) << L" - ";
    if (g_transitionActive && g_transitionTargetBrightness >= 0) {
        tip << (ShouldUseChineseUi() ? L"SDR 内容亮度恢复中" : L"Restoring SDR content brightness") << L" "
            << g_transitionTargetBrightness << (ShouldUseChineseUi() ? L"％" : L"%");
    } else if (g_lastAppliedBrightness >= 0) {
        tip << (ShouldUseChineseUi() ? L"SDR 内容亮度 " : L"SDR content brightness ")
            << (g_lastDecisionNight ? T(TxtNight) : T(TxtDay)) << L" "
            << g_lastAppliedBrightness << (ShouldUseChineseUi() ? L"％" : L"%");
    } else {
        tip << T(TxtStarting);
    }

    tip << L" - ";
    if (g_lastHdrTargetCount > 0) {
        if (ShouldUseChineseUi()) {
            tip << L"HDR 屏幕 " << g_lastHdrSuccessCount << L"/" << g_lastHdrTargetCount;
        } else {
            tip << L"HDR " << g_lastHdrSuccessCount << L"/" << g_lastHdrTargetCount;
        }
    } else {
        tip << (ShouldUseChineseUi() ? L"无 HDR" : L"No HDR");
    }

    tip << L" - ";
    tip << (ShouldUseChineseUi() ? L"自动纠正" : L"Restore") << L" "
        << (g_config.autoRestoreManualChanges ? (ShouldUseChineseUi() ? L"开" : L"on")
                                              : (ShouldUseChineseUi() ? L"关" : L"off"));

    g_tray.uFlags = NIF_TIP | NIF_SHOWTIP;
    CopyString(g_tray.szTip, sizeof(g_tray.szTip) / sizeof(g_tray.szTip[0]), tip.str());
    Shell_NotifyIconW(NIM_MODIFY, &g_tray);
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
}

void ShowTrayNotification(const std::wstring& title, const std::wstring& body) {
    if (!g_mainWindow) return;
    g_lastNotificationTitle = title;
    g_lastNotificationBody = body;

    g_tray.uFlags = NIF_INFO;
    CopyString(g_tray.szInfoTitle, sizeof(g_tray.szInfoTitle) / sizeof(g_tray.szInfoTitle[0]), title);
    CopyString(g_tray.szInfo, sizeof(g_tray.szInfo) / sizeof(g_tray.szInfo[0]), body);
    g_tray.dwInfoFlags = NIIF_INFO;
    g_tray.uTimeout = 5000;
    Shell_NotifyIconW(NIM_MODIFY, &g_tray);
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
}

void ShowLastNotificationDialog() {
    if (g_lastNotificationBody.empty()) return;
    MessageBoxW(g_mainWindow, g_lastNotificationBody.c_str(),
                g_lastNotificationTitle.empty() ? T(TxtNotifyDialogTitle) : g_lastNotificationTitle.c_str(),
                MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
}

void NotifyManualCorrection(int brightness) {
    DWORD now = GetTickCount();
    if (g_lastManualNotificationTick != 0 && now - g_lastManualNotificationTick < 30000) return;
    g_lastManualNotificationTick = now;

    std::wstringstream body;
    if (ShouldUseChineseUi()) {
        body << T(TxtNotifyBody) << PercentLabel(brightness) << L"。";
        body << L"如果不想让软件干涉手动调整，请在设置中关闭“"
             << T(TxtAutoRestoreManual) << L"”。";
    } else {
        body << T(TxtNotifyBody) << L" " << PercentLabel(brightness) << L". ";
        body << L"To stop the app from interfering with manual adjustments, open Settings and turn off \""
             << T(TxtAutoRestoreManual) << L"\".";
    }
    ShowTrayNotification(T(TxtNotifyTitle), body.str());
}

void StopBrightnessTransition() {
    if (g_mainWindow) KillTimer(g_mainWindow, kTransitionTimer);
    g_transitionActive = false;
    g_transitionManualCorrection = false;
}

void ContinueBrightnessTransition() {
    if (!g_transitionActive) return;

    ApplyResult result = ApplySdrLevelStep(g_transitionTargetLevel, true);
    NightDecision decision;
    decision.night = g_transitionNight;
    decision.source = g_transitionSource;

    if (!result.ok) {
        StopBrightnessTransition();
        g_lastHdrTargetCount = result.targetCount;
        g_lastHdrSuccessCount = result.successCount;
        g_status = BuildStatusText(decision, g_transitionTargetBrightness, result);
        UpdateTrayTip();
        return;
    }

    if (result.complete) {
        StopBrightnessTransition();
        g_lastAppliedBrightness = g_transitionTargetBrightness;
        g_lastDecisionNight = g_transitionNight;
        g_lastKnownTargetLevel = g_transitionTargetLevel;
        g_lastHdrTargetCount = result.targetCount;
        g_lastHdrSuccessCount = result.successCount;
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
    LoadConfig();
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
        g_status = BuildStatusText(decision, brightness, check);
        UpdateTrayTip();
        return;
    }

    bool manualCorrection = !force && !targetChanged && check.targetCount > 0 && !check.complete;
    if (manualCorrection && !g_config.autoRestoreManualChanges) {
        g_lastHdrTargetCount = check.targetCount;
        g_lastHdrSuccessCount = check.successCount;
        g_status = T(TxtManualRestoreOff);
        UpdateTrayTip();
        return;
    }
    BeginBrightnessTransition(decision, brightness, manualCorrection);
    UpdateTrayTip();
}

void AddTrayIcon(HWND hwnd) {
    ZeroMemory(&g_tray, sizeof(g_tray));
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = hwnd;
    g_tray.uID = 1;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    g_tray.uCallbackMessage = kTrayMessage;
    g_tray.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON));
    if (!g_tray.hIcon) g_tray.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    CopyString(g_tray.szTip, sizeof(g_tray.szTip) / sizeof(g_tray.szTip[0]), T(TxtDisplayName));
    Shell_NotifyIconW(NIM_ADD, &g_tray);
    g_tray.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_tray);
}

void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_tray);
}

void AppendMenuText(HMENU menu, UINT flags, UINT_PTR id, const std::wstring& text) {
    AppendMenuW(menu, flags, id, text.c_str());
}

COLORREF Rgb(BYTE r, BYTE g, BYTE b) {
    return RGB(r, g, b);
}

bool ShouldUseDarkAppTheme() {
    DWORD lightTheme = 1;
    ReadDwordValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   L"AppsUseLightTheme", &lightTheme);
    return lightTheme == 0;
}

UiTheme BuildUiTheme() {
    UiTheme theme = {};
    theme.dark = ShouldUseDarkAppTheme();
    theme.primary = Rgb(0, 120, 212);
    theme.primaryHover = Rgb(8, 134, 232);
    theme.primaryBorderHover = Rgb(75, 190, 255);

    if (theme.dark) {
        theme.window = Rgb(15, 17, 19);
        theme.card = Rgb(28, 33, 36);
        theme.cardBorder = Rgb(43, 50, 56);
        theme.cardHover = Rgb(33, 39, 43);
        theme.elevated = Rgb(39, 46, 51);
        theme.control = Rgb(32, 38, 42);
        theme.controlHover = Rgb(45, 53, 59);
        theme.controlBorder = Rgb(50, 58, 64);
        theme.controlBorderHover = Rgb(84, 155, 210);
        theme.text = Rgb(232, 236, 240);
        theme.mutedText = Rgb(170, 178, 186);
        theme.titleText = Rgb(245, 247, 250);
        theme.track = Rgb(63, 72, 80);
        theme.trackHover = Rgb(74, 85, 94);
        theme.knob = Rgb(245, 247, 250);
        theme.disabledText = Rgb(112, 121, 130);
    } else {
        theme.window = Rgb(243, 243, 243);
        theme.card = Rgb(255, 255, 255);
        theme.cardBorder = Rgb(224, 228, 234);
        theme.cardHover = Rgb(246, 248, 251);
        theme.elevated = Rgb(246, 248, 251);
        theme.control = Rgb(246, 248, 251);
        theme.controlHover = Rgb(235, 241, 247);
        theme.controlBorder = Rgb(216, 222, 230);
        theme.controlBorderHover = Rgb(110, 177, 225);
        theme.text = Rgb(32, 32, 32);
        theme.mutedText = Rgb(96, 103, 112);
        theme.titleText = Rgb(24, 24, 24);
        theme.track = Rgb(211, 218, 226);
        theme.trackHover = Rgb(198, 207, 217);
        theme.knob = Rgb(255, 255, 255);
        theme.disabledText = Rgb(145, 151, 158);
    }
    return theme;
}

void ReloadUiTheme() {
    g_theme = BuildUiTheme();
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

int Ui(int value) {
    HDC dc = GetDC(NULL);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(NULL, dc);
    return MulDiv(value, dpi, 96);
}

RECT UiBox(int x, int y, int width, int height) {
    RECT rect = {Ui(x), Ui(y), Ui(x + width), Ui(y + height)};
    return rect;
}

bool PtInUiBox(POINT pt, int x, int y, int width, int height) {
    RECT rect = UiBox(x, y, width, height);
    return PtInRect(&rect, pt) != FALSE;
}

bool IsHover(int control) {
    return g_hoverControl == control;
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

std::wstring TimeText(int hour, int minute) {
    std::wstringstream ss;
    ss << FormatTwoDigit(hour) << L":" << FormatTwoDigit(minute);
    return ss.str();
}

void AddMinutesToTime(int* hour, int* minute, int delta) {
    int total = ((*hour * 60 + *minute + delta) % (24 * 60) + (24 * 60)) % (24 * 60);
    *hour = total / 60;
    *minute = total % 60;
}

HFONT CreateUiFont(int pointSize, int weight) {
    HDC dc = GetDC(NULL);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) ReleaseDC(NULL, dc);
    int height = -MulDiv(pointSize, dpi, 72);
    const wchar_t* primaryFace = ShouldUseChineseUi() ? L"Microsoft YaHei UI" : L"Segoe UI Variable Text";
    const wchar_t* fallbackFace = ShouldUseChineseUi() ? L"Segoe UI" : L"Segoe UI";
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

void EnsureUiResources() {
    if (g_theme.window == 0 && g_theme.card == 0) {
        g_theme = BuildUiTheme();
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
    HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
    if (!dwmapi) return;

    typedef HRESULT(WINAPI* DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
    DwmSetWindowAttributeFn setAttribute =
        reinterpret_cast<DwmSetWindowAttributeFn>(GetProcAddress(dwmapi, "DwmSetWindowAttribute"));
    if (setAttribute) {
        BOOL darkFrame = g_theme.dark ? TRUE : FALSE;
        setAttribute(hwnd, 20, &darkFrame, sizeof(darkFrame));  // DWMWA_USE_IMMERSIVE_DARK_MODE on recent builds.
        setAttribute(hwnd, 19, &darkFrame, sizeof(darkFrame));  // Older dark mode attribute.

        DWORD corner = 2;  // DWMWCP_ROUND.
        setAttribute(hwnd, 33, &corner, sizeof(corner));

        DWORD backdrop = 2;  // DWMSBT_MAINWINDOW (Mica) on Windows 11 22H2+.
        setAttribute(hwnd, 38, &backdrop, sizeof(backdrop));

        COLORREF caption = g_theme.dark ? Rgb(12, 14, 16) : Rgb(243, 243, 243);
        COLORREF text = g_theme.titleText;
        setAttribute(hwnd, 35, &caption, sizeof(caption));  // DWMWA_CAPTION_COLOR on Windows 11.
        setAttribute(hwnd, 36, &text, sizeof(text));        // DWMWA_TEXT_COLOR on Windows 11.
    }

    FreeLibrary(dwmapi);
}

void AddRoundedRectPath(Gdiplus::GraphicsPath* path, Gdiplus::REAL x, Gdiplus::REAL y,
                        Gdiplus::REAL w, Gdiplus::REAL h, Gdiplus::REAL radius) {
    radius = std::max<Gdiplus::REAL>(0.0f, std::min<Gdiplus::REAL>(radius, std::min(w, h) / 2.0f));
    Gdiplus::REAL d = radius * 2.0f;
    if (radius <= 0.0f) {
        path->AddRectangle(Gdiplus::RectF(x, y, w, h));
        return;
    }
    path->AddArc(x, y, d, d, 180.0f, 90.0f);
    path->AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path->AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path->AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path->CloseFigure();
}

void ShowTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    std::wstring status = g_status.empty() ? T(TxtStarting) : g_status;
    AppendMenuText(menu, MF_STRING | MF_DISABLED, 0, status);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, kMenuApply, T(TxtMenuApply));
    AppendMenuW(menu, MF_STRING, kMenuSettings, T(TxtMenuSettings));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING | (IsStartupEnabled() ? MF_CHECKED : 0), kMenuStartup, T(TxtMenuStartup));
    AppendMenuW(menu, MF_STRING, kMenuDisplaySettings, T(TxtMenuDisplaySettings));
    AppendMenuW(menu, MF_STRING, kMenuNightLightSettings, T(TxtMenuNightLightSettings));
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
    g_config = next;
    SaveConfig();
    PostMessageW(g_mainWindow, kApplyMessage, TRUE, 0);
    if (closeWindow) DestroyWindow(hwnd);
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
    int brightnessTop;
    int brightnessH;
    int brightnessRow1;
    int brightnessRow2;
    int switchTop;
    int switchH;
    int switchModeX;
    int switchModeY;
    int switchModeW;
    int switchModeH;
    int switchHintY;
    int switchNightY;
    int switchDayY;
    int behaviorTop;
    int behaviorH;
    int behaviorRow1;
    int behaviorRow2;
    int footerY;
};

SettingsLayout BuildSettingsLayout(bool useSystemSwitching) {
    SettingsLayout layout = {};
    const int cardGap = 12;
    const int contentBottomPad = 24;

    layout.cardX = 28;
    layout.cardW = 584;

    layout.headerIconX = 28;
    layout.headerIconY = 22;
    layout.headerTitleX = 78;
    layout.headerTitleY = 18;
    layout.headerSubtitleY = 48;

    layout.languageSegmentX = 402;
    layout.languageSegmentY = 24;
    layout.languageSegmentW = 190;
    layout.languageSegmentH = 32;
    layout.languageTop = layout.languageSegmentY;
    layout.languageH = layout.languageSegmentH;

    layout.heroTop = 82;
    layout.heroH = 116;

    layout.brightnessTop = layout.heroTop + layout.heroH + cardGap;
    layout.brightnessH = 132;
    layout.brightnessRow1 = layout.brightnessTop + 54;
    layout.brightnessRow2 = layout.brightnessRow1 + 38;

    layout.switchTop = layout.brightnessTop + layout.brightnessH + cardGap;
    layout.switchH = useSystemSwitching ? 104 : 148;
    layout.switchModeX = 372;
    layout.switchModeY = layout.switchTop + 18;
    layout.switchModeW = 220;
    layout.switchModeH = 32;
    layout.switchHintY = layout.switchTop + 64;
    layout.switchNightY = layout.switchTop + 62;
    layout.switchDayY = layout.switchNightY + 38;

    layout.behaviorTop = layout.switchTop + layout.switchH + cardGap;
    layout.behaviorH = 132;
    layout.behaviorRow1 = layout.behaviorTop + 52;
    layout.behaviorRow2 = layout.behaviorRow1 + 40;

    layout.footerY = layout.behaviorTop + layout.behaviorH + 18;
    layout.clientHeight = std::max(kSettingsClientHeight, layout.footerY + 34 + contentBottomPad);
    return layout;
}

bool SettingsDraftUsesSystemSwitching() {
    return g_settingsDraft.followNightLight && CanFollowWindowsNightLight();
}

void ResizeSettingsWindowToLayout(HWND hwnd) {
    SettingsLayout layout = BuildSettingsLayout(SettingsDraftUsesSystemSwitching());
    DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    SIZE size = WindowSizeForClient(style, exStyle, Ui(kSettingsClientWidth), Ui(layout.clientHeight));
    SetWindowPos(hwnd, NULL, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

int HitTestSettingsControl(POINT pt) {
    bool canFollow = CanFollowWindowsNightLight();
    bool useSystem = g_settingsDraft.followNightLight && canFollow;
    SettingsLayout layout = BuildSettingsLayout(useSystem);

    if (PtInUiBox(pt, layout.languageSegmentX, layout.languageSegmentY, layout.languageSegmentW, layout.languageSegmentH)) {
        int localX = pt.x - Ui(layout.languageSegmentX);
        int segment = ClampInt(localX / std::max(1, Ui(layout.languageSegmentW / 3)), 0, 2);
        return HoverLanguageBase + segment;
    }

    if (PtInUiBox(pt, 190, layout.brightnessRow1 - 22, 340, 40)) return HoverDaySlider;
    if (PtInUiBox(pt, 190, layout.brightnessRow2 - 22, 340, 40)) return HoverNightSlider;

    if (PtInUiBox(pt, layout.switchModeX, layout.switchModeY, layout.switchModeW, layout.switchModeH)) {
        int localX = pt.x - Ui(layout.switchModeX);
        int segment = ClampInt(localX / std::max(1, Ui(layout.switchModeW / 2)), 0, 1);
        return HoverSwitchBase + segment;
    }

    if (PtInUiBox(pt, 58, layout.behaviorRow1 - 4, 580, 34)) return HoverAutoRestore;
    if (PtInUiBox(pt, 58, layout.behaviorRow2 - 4, 580, 34)) return HoverStartup;

    if (!useSystem && PtInUiBox(pt, 390, layout.switchNightY - 4, 28, 28)) return HoverNightMinus;
    if (!useSystem && PtInUiBox(pt, 522, layout.switchNightY - 4, 28, 28)) return HoverNightPlus;
    if (!useSystem && PtInUiBox(pt, 390, layout.switchDayY - 4, 28, 28)) return HoverDayMinus;
    if (!useSystem && PtInUiBox(pt, 522, layout.switchDayY - 4, 28, 28)) return HoverDayPlus;

    if (PtInUiBox(pt, 356, layout.footerY, 76, 34)) return HoverOk;
    if (PtInUiBox(pt, 446, layout.footerY, 76, 34)) return HoverApply;
    if (PtInUiBox(pt, 536, layout.footerY, 76, 34)) return HoverCancel;
    return HoverNone;
}

void UpdateSettingsHover(HWND hwnd, POINT pt) {
    if (!g_trackingSettingsMouse) {
        TRACKMOUSEEVENT track = {};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = hwnd;
        TrackMouseEvent(&track);
        g_trackingSettingsMouse = true;
    }

    int nextHover = HitTestSettingsControl(pt);
    if (nextHover != g_hoverControl) {
        g_hoverControl = nextHover;
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

const wchar_t* UiText(const wchar_t* en, const wchar_t* zh) {
    return ShouldUseChineseUi() ? zh : en;
}

std::wstring PercentLabel(int value) {
    std::wstringstream ss;
    ss << value << (ShouldUseChineseUi() ? L"％" : L"%");
    return ss.str();
}

std::wstring HdrSyncText() {
    if (g_lastHdrTargetCount <= 0) {
        return ShouldUseChineseUi() ? L"未检测到 HDR 屏幕" : std::wstring(T(TxtNoHdrDisplay));
    }

    std::wstringstream ss;
    if (ShouldUseChineseUi()) {
        if (g_lastHdrSuccessCount >= g_lastHdrTargetCount) {
            ss << L"已同步 " << g_lastHdrTargetCount << L" 台 HDR 屏幕";
        } else {
            ss << L"已同步 " << g_lastHdrSuccessCount << L" 台，共 " << g_lastHdrTargetCount << L" 台 HDR 屏幕";
        }
    } else {
        ss << L"HDR " << g_lastHdrSuccessCount << L"/" << g_lastHdrTargetCount;
    }
    return ss.str();
}

std::wstring HdrPillText() {
    if (g_lastHdrTargetCount <= 0) {
        return ShouldUseChineseUi() ? L"无 HDR 屏幕" : std::wstring(T(TxtNoHdrDisplay));
    }

    std::wstringstream ss;
    if (ShouldUseChineseUi()) {
        ss << L"已同步 " << g_lastHdrSuccessCount << L" 台";
    } else {
        ss << L"HDR " << g_lastHdrSuccessCount << L"/" << g_lastHdrTargetCount;
    }
    return ss.str();
}

std::wstring HeroStatusText() {
    if (g_lastAppliedBrightness < 0) {
        return T(TxtSettingsSubtitle);
    }

    std::wstringstream ss;
    if (ShouldUseChineseUi()) {
        ss << L"SDR 内容亮度：" << (g_lastDecisionNight ? L"夜间" : L"白天")
           << L" " << PercentLabel(g_lastAppliedBrightness) << L"，" << HdrSyncText() << L"。";
    } else {
        ss << L"SDR content brightness: " << PercentLabel(g_lastAppliedBrightness) << L" for "
           << (g_lastDecisionNight ? L"night" : L"day") << L". " << HdrSyncText() << L".";
    }
    return ss.str();
}

void DrawAppMark(HDC dc, int x, int y) {
    DrawCircleFill(dc, x, y, 40, g_theme.dark ? Rgb(4, 6, 7) : Rgb(255, 255, 255),
                   g_theme.dark ? Rgb(36, 43, 48) : Rgb(213, 219, 227));
    DrawCircleFill(dc, x + 13, y + 10, 5, Rgb(56, 189, 248), Rgb(56, 189, 248));
    DrawCircleFill(dc, x + 22, y + 10, 5, Rgb(34, 197, 94), Rgb(34, 197, 94));
    DrawCircleFill(dc, x + 13, y + 19, 5, Rgb(14, 165, 233), Rgb(14, 165, 233));
    DrawCircleFill(dc, x + 22, y + 19, 5, Rgb(245, 158, 11), Rgb(245, 158, 11));
    DrawRoundedFill(dc, x + 11, y + 29, 18, 3, 2, Rgb(0, 120, 212), Rgb(0, 120, 212));
}

void DrawStatusPill(HDC dc, int x, int y, int w, const std::wstring& text, COLORREF accent) {
    DrawRoundedFill(dc, x, y, w, 28, 14, g_theme.elevated, g_theme.controlBorder);
    DrawCircleFill(dc, x + 12, y + 10, 8, accent, accent);
    RECT rect = UiBox(x + 26, y, w - 34, 28);
    DrawTextLine(dc, text.c_str(), rect, g_smallFont, g_theme.text,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawSegmentedLanguage(HDC dc, const SettingsLayout& layout) {
    const int x = layout.languageSegmentX;
    const int y = layout.languageSegmentY;
    const int w = layout.languageSegmentW;
    const int h = layout.languageSegmentH;
    const int segmentW = w / 3;
    DrawRoundedFill(dc, x, y, w, h, 16, g_theme.control, g_theme.controlBorder);

    int selected = ClampInt(g_settingsDraft.language, LangAuto, LangChinese);
    int hoverSegment = HoverSegment(HoverLanguageBase, 3);
    if (hoverSegment >= 0 && hoverSegment != selected) {
        DrawRoundedFill(dc, x + hoverSegment * segmentW + 2, y + 2, segmentW - 4, h - 4, 14,
                        g_theme.controlHover, g_theme.controlBorder);
    }
    COLORREF selectedFill = hoverSegment == selected ? g_theme.primaryHover : g_theme.primary;
    DrawRoundedFill(dc, x + selected * segmentW + 2, y + 2, segmentW - 4, h - 4, 14, selectedFill, selectedFill);

    const wchar_t* labels[3] = {T(TxtLanguageAuto), T(TxtLanguageEnglish), T(TxtLanguageChinese)};
    for (int i = 0; i < 3; ++i) {
        RECT rect = UiBox(x + i * segmentW, y, segmentW, h);
        DrawTextLine(dc, labels[i], rect, g_smallFont, i == selected ? Rgb(255, 255, 255) : g_theme.text,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void DrawSegmentedSwitchMode(HDC dc, const SettingsLayout& layout, bool canFollow) {
    const int x = layout.switchModeX;
    const int y = layout.switchModeY;
    const int w = layout.switchModeW;
    const int h = layout.switchModeH;
    const int segmentW = w / 2;
    bool useSystem = g_settingsDraft.followNightLight && canFollow;
    int selected = useSystem ? 0 : 1;

    DrawRoundedFill(dc, x, y, w, h, 16, g_theme.control, g_theme.controlBorder);
    int hoverSegment = HoverSegment(HoverSwitchBase, 2);
    if (hoverSegment >= 0 && hoverSegment != selected) {
        DrawRoundedFill(dc, x + hoverSegment * segmentW + 2, y + 2, segmentW - 4, h - 4, 14,
                        g_theme.controlHover, g_theme.controlBorder);
    }
    COLORREF selectedFill = hoverSegment == selected ? g_theme.primaryHover : g_theme.primary;
    DrawRoundedFill(dc, x + selected * segmentW + 2, y + 2, segmentW - 4, h - 4, 14, selectedFill, selectedFill);

    COLORREF disabled = g_theme.disabledText;
    RECT followRect = UiBox(x, y, segmentW, h);
    RECT defaultRect = UiBox(x + segmentW, y, segmentW, h);
    DrawTextLine(dc, T(TxtFollowSystem), followRect, g_uiFont,
                 selected == 0 ? Rgb(255, 255, 255) : (canFollow ? g_theme.text : disabled),
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextLine(dc, T(TxtUseDefaultSchedule), defaultRect, g_uiFont,
                 selected == 1 ? Rgb(255, 255, 255) : g_theme.text,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawValuePill(HDC dc, int x, int y, int value) {
    DrawRoundedFill(dc, x, y, 56, 30, 15, g_theme.control, g_theme.controlBorder);
    std::wstring text = PercentLabel(value);
    RECT rect = UiBox(x, y, 56, 30);
    DrawTextLine(dc, text.c_str(), rect, g_sectionFont, g_theme.titleText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawSlider(HDC dc, int x, int y, int w, int value, bool hovered) {
    int trackY = y + 15;
    DrawRoundedFill(dc, x, trackY, w, hovered ? 6 : 5, 3,
                    hovered ? g_theme.trackHover : g_theme.track,
                    hovered ? g_theme.trackHover : g_theme.track);
    int fillW = MulDiv(w, ClampInt(value, 0, 100), 100);
    if (fillW > 0) {
        DrawRoundedFill(dc, x, trackY, fillW, hovered ? 6 : 5, 3,
                        hovered ? g_theme.primaryHover : g_theme.primary,
                        hovered ? g_theme.primaryHover : g_theme.primary);
    }

    int knobSize = hovered ? 22 : 18;
    int knobX = x + fillW - knobSize / 2;
    DrawCircleFill(dc, knobX, hovered ? y + 5 : y + 7, knobSize, g_theme.knob,
                   hovered ? g_theme.primaryBorderHover : Rgb(102, 192, 255));
}

void DrawToggle(HDC dc, int x, int y, bool checked, bool hovered) {
    COLORREF fill = checked ? (hovered ? g_theme.primaryHover : g_theme.primary)
                            : (hovered ? g_theme.controlHover : g_theme.control);
    COLORREF border = checked ? (hovered ? g_theme.primaryBorderHover : g_theme.primary)
                              : (hovered ? g_theme.controlBorderHover : g_theme.controlBorder);
    DrawRoundedFill(dc, x, y, 44, 24, 12, fill, border);
    int knobX = checked ? x + 22 : x + 2;
    DrawCircleFill(dc, knobX, y + 2, 20, Rgb(255, 255, 255), Rgb(255, 255, 255));
}

void DrawSettingRowText(HDC dc, const wchar_t* text, int x, int y, int w) {
    RECT rect = UiBox(x, y, w, 28);
    DrawTextLine(dc, text, rect, g_uiFont, g_theme.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawTimeStepper(HDC dc, const wchar_t* label, int y, int hour, int minute, bool minusHover, bool plusHover) {
    DrawSettingRowText(dc, label, 64, y, 210);
    DrawRoundedFill(dc, 390, y - 4, 28, 28, 14,
                    minusHover ? g_theme.controlHover : g_theme.control,
                    minusHover ? g_theme.controlBorderHover : g_theme.controlBorder);
    DrawRoundedFill(dc, 522, y - 4, 28, 28, 14,
                    plusHover ? g_theme.controlHover : g_theme.control,
                    plusHover ? g_theme.controlBorderHover : g_theme.controlBorder);
    DrawRoundedFill(dc, 426, y - 6, 88, 32, 16, g_theme.elevated, g_theme.controlBorder);

    RECT minusRect = UiBox(390, y - 4, 28, 28);
    RECT plusRect = UiBox(522, y - 4, 28, 28);
    RECT timeRect = UiBox(426, y - 6, 88, 32);
    std::wstring time = TimeText(hour, minute);
    DrawTextLine(dc, L"-", minusRect, g_sectionFont, g_theme.titleText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextLine(dc, time.c_str(), timeRect, g_sectionFont, g_theme.titleText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextLine(dc, L"+", plusRect, g_sectionFont, g_theme.titleText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawFooterButton(HDC dc, int x, int y, int w, const wchar_t* text, bool primary, bool hovered) {
    COLORREF fill = primary ? (hovered ? g_theme.primaryHover : g_theme.primary)
                            : (hovered ? g_theme.controlHover : g_theme.window);
    COLORREF border = primary ? (hovered ? g_theme.primaryBorderHover : g_theme.primary)
                              : (hovered ? g_theme.controlBorderHover : g_theme.controlBorder);
    COLORREF color = primary ? Rgb(255, 255, 255) : g_theme.text;
    DrawRoundedFill(dc, x, y, w, 34, 7, fill, border);
    RECT rect = UiBox(x, y, w, 34);
    DrawTextLine(dc, text, rect, g_sectionFont, color, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawHeroCard(HDC dc, const SettingsLayout& layout) {
    const int x = layout.cardX;
    const int y = layout.heroTop;
    const int w = layout.cardW;
    DrawRoundedFill(dc, x, y, w, layout.heroH, 8, g_theme.card, g_theme.cardBorder);

    std::wstring mode = g_lastAppliedBrightness >= 0
                            ? std::wstring(g_lastDecisionNight ? T(TxtNight) : T(TxtDay))
                            : std::wstring(T(TxtStarting));
    std::wstring level = g_lastAppliedBrightness >= 0 ? PercentLabel(g_lastAppliedBrightness)
                                                       : (ShouldUseChineseUi() ? L"--％" : L"--%");
    std::wstring headline = mode + L"  " + level;

    RECT labelRect = UiBox(x + 28, y + 18, 230, 22);
    DrawTextLine(dc, UiText(L"Current state", L"当前状态"), labelRect, g_sectionFont, g_theme.titleText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT headlineRect = UiBox(x + 28, y + 46, 360, 34);
    DrawTextLine(dc, headline.c_str(), headlineRect, g_heroFont, g_theme.titleText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    std::wstring status = HeroStatusText();
    RECT statusRect = UiBox(x + 28, y + 84, 390, 22);
    DrawTextLine(dc, status.c_str(), statusRect, g_smallFont, g_theme.mutedText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    DrawStatusPill(dc, x + w - 176, y + 26, 148, HdrPillText(),
                   g_lastHdrTargetCount > 0 ? Rgb(34, 197, 94) : Rgb(245, 158, 11));
    DrawStatusPill(dc, x + w - 176, y + 64, 148,
                   g_settingsDraft.autoRestoreManualChanges ? UiText(L"Restore on", L"纠正开启")
                                                            : UiText(L"Restore off", L"纠正关闭"),
                   g_settingsDraft.autoRestoreManualChanges ? Rgb(34, 197, 94) : Rgb(148, 163, 184));
}

void DrawSettingsChrome(HWND hwnd, HDC dc) {
    EnsureUiResources();
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(dc, &client, g_windowBrush);

    const int sectionX = 56;
    const int rowX = 64;
    const int titlePadY = 17;
    const int controlX = 548;
    bool canFollow = CanFollowWindowsNightLight();
    bool useSystem = g_settingsDraft.followNightLight && canFollow;
    SettingsLayout layout = BuildSettingsLayout(useSystem);

    DrawAppMark(dc, layout.headerIconX, layout.headerIconY);
    RECT title = UiBox(layout.headerTitleX, layout.headerTitleY, 300, 30);
    DrawTextLine(dc, T(TxtDisplayName), title, g_titleFont, g_theme.titleText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT subtitle = UiBox(layout.headerTitleX, layout.headerSubtitleY, 300, 22);
    DrawTextLine(dc, T(TxtSettingsSubtitle), subtitle, g_smallFont, g_theme.mutedText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    DrawSegmentedLanguage(dc, layout);

    DrawHeroCard(dc, layout);

    DrawRoundedFill(dc, layout.cardX, layout.brightnessTop, layout.cardW, layout.brightnessH, 8, g_theme.card, g_theme.cardBorder);
    DrawRoundedFill(dc, layout.cardX, layout.switchTop, layout.cardW, layout.switchH, 8, g_theme.card, g_theme.cardBorder);
    DrawRoundedFill(dc, layout.cardX, layout.behaviorTop, layout.cardW, layout.behaviorH, 8, g_theme.card, g_theme.cardBorder);

    int sectionY = layout.brightnessTop + titlePadY;
    int row1 = layout.brightnessRow1;
    int row2 = layout.brightnessRow2;
    RECT section = UiBox(sectionX, sectionY, 240, 24);
    DrawTextLine(dc, T(TxtBrightnessGroup), section, g_sectionFont, g_theme.titleText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawSettingRowText(dc, T(TxtDay), rowX, row1, 120);
    DrawSlider(dc, 210, row1 - 2, 300, g_settingsDraft.dayBrightness, IsHover(HoverDaySlider));
    DrawValuePill(dc, 536, row1 - 9, g_settingsDraft.dayBrightness);
    DrawSettingRowText(dc, T(TxtNight), rowX, row2, 120);
    DrawSlider(dc, 210, row2 - 2, 300, g_settingsDraft.nightBrightness, IsHover(HoverNightSlider));
    DrawValuePill(dc, 536, row2 - 9, g_settingsDraft.nightBrightness);

    sectionY = layout.switchTop + titlePadY;
    section = UiBox(sectionX, sectionY, 240, 24);
    DrawTextLine(dc, T(TxtSwitchMode), section, g_sectionFont, g_theme.titleText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawSegmentedSwitchMode(dc, layout, canFollow);
    if (useSystem) {
        RECT hintRect = UiBox(rowX, layout.switchHintY, 500, 28);
        DrawTextLine(dc, T(TxtFollowSystemHint), hintRect, g_uiFont, g_theme.mutedText,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        DrawTimeStepper(dc, T(TxtNightStarts), layout.switchNightY, g_settingsDraft.nightStartHour, g_settingsDraft.nightStartMinute,
                        IsHover(HoverNightMinus), IsHover(HoverNightPlus));
        DrawTimeStepper(dc, T(TxtDayStarts), layout.switchDayY, g_settingsDraft.dayStartHour, g_settingsDraft.dayStartMinute,
                        IsHover(HoverDayMinus), IsHover(HoverDayPlus));
    }

    sectionY = layout.behaviorTop + titlePadY;
    row1 = layout.behaviorRow1;
    row2 = layout.behaviorRow2;
    section = UiBox(sectionX, sectionY, 240, 24);
    DrawTextLine(dc, T(TxtBehaviorGroup), section, g_sectionFont, g_theme.titleText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (IsHover(HoverAutoRestore)) {
        DrawRoundedFill(dc, 54, row1 - 3, 528, 34, 7, g_theme.cardHover, g_theme.cardHover);
    }
    if (IsHover(HoverStartup)) {
        DrawRoundedFill(dc, 54, row2 - 3, 528, 34, 7, g_theme.cardHover, g_theme.cardHover);
    }
    DrawSettingRowText(dc, T(TxtAutoRestoreManual), rowX, row1, 440);
    DrawToggle(dc, controlX, row1 + 2, g_settingsDraft.autoRestoreManualChanges, IsHover(HoverAutoRestore));
    DrawSettingRowText(dc, T(TxtStartWithWindows), rowX, row2, 440);
    DrawToggle(dc, controlX, row2 + 2, g_settingsDraft.startWithWindows, IsHover(HoverStartup));

    DrawFooterButton(dc, 356, layout.footerY, 76, T(TxtOk), true, IsHover(HoverOk));
    DrawFooterButton(dc, 446, layout.footerY, 76, T(TxtApply), false, IsHover(HoverApply));
    DrawFooterButton(dc, 536, layout.footerY, 76, T(TxtCancel), false, IsHover(HoverCancel));
}

void ApplySettingsDraft(HWND hwnd, bool closeWindow) {
    if (g_settingsDraft.followNightLight && !CanFollowWindowsNightLight()) {
        g_settingsDraft.followNightLight = false;
    }
    g_config = g_settingsDraft;
    SaveConfig();
    PostMessageW(g_mainWindow, kApplyMessage, TRUE, 0);
    if (closeWindow) {
        DestroyWindow(hwnd);
    } else {
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateTrayTip();
    }
}

void SetDraftBrightnessFromPoint(HWND hwnd, POINT pt, int id) {
    int value = SliderValueFromPoint(pt, 210, 300);
    if (id == kIdDayBrightness) {
        g_settingsDraft.dayBrightness = value;
    } else if (id == kIdNightBrightness) {
        g_settingsDraft.nightBrightness = value;
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

void HandleSettingsClick(HWND hwnd, POINT pt) {
    bool canFollow = CanFollowWindowsNightLight();
    bool useSystem = g_settingsDraft.followNightLight && canFollow;
    SettingsLayout layout = BuildSettingsLayout(useSystem);

    if (PtInUiBox(pt, layout.languageSegmentX, layout.languageSegmentY, layout.languageSegmentW, layout.languageSegmentH)) {
        int localX = pt.x - Ui(layout.languageSegmentX);
        int segment = ClampInt(localX / std::max(1, Ui(layout.languageSegmentW / 3)), LangAuto, LangChinese);
        g_settingsDraft.language = segment;
        CleanupFontResources();
        EnsureUiResources();
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    if (PtInUiBox(pt, 190, layout.brightnessRow1 - 22, 340, 40)) {
        g_draggingBrightnessId = kIdDayBrightness;
        SetCapture(hwnd);
        SetDraftBrightnessFromPoint(hwnd, pt, g_draggingBrightnessId);
        return;
    }
    if (PtInUiBox(pt, 190, layout.brightnessRow2 - 22, 340, 40)) {
        g_draggingBrightnessId = kIdNightBrightness;
        SetCapture(hwnd);
        SetDraftBrightnessFromPoint(hwnd, pt, g_draggingBrightnessId);
        return;
    }

    if (PtInUiBox(pt, layout.switchModeX, layout.switchModeY, layout.switchModeW, layout.switchModeH)) {
        int localX = pt.x - Ui(layout.switchModeX);
        int segment = ClampInt(localX / std::max(1, Ui(layout.switchModeW / 2)), 0, 1);
        if (segment == 0) {
            if (canFollow) {
                g_settingsDraft.followNightLight = true;
            } else {
                MessageBoxW(hwnd, T(TxtNightLightUnavailable), T(TxtDisplayName),
                            MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
                g_settingsDraft.followNightLight = false;
            }
        } else {
            g_settingsDraft.followNightLight = false;
        }
        ResizeSettingsWindowToLayout(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    if (PtInUiBox(pt, 58, layout.behaviorRow1 - 4, 580, 34)) {
        g_settingsDraft.autoRestoreManualChanges = !g_settingsDraft.autoRestoreManualChanges;
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (PtInUiBox(pt, 58, layout.behaviorRow2 - 4, 580, 34)) {
        g_settingsDraft.startWithWindows = !g_settingsDraft.startWithWindows;
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    if (!useSystem && PtInUiBox(pt, 390, layout.switchNightY - 4, 28, 28)) {
        AddMinutesToTime(&g_settingsDraft.nightStartHour, &g_settingsDraft.nightStartMinute, -30);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (!useSystem && PtInUiBox(pt, 522, layout.switchNightY - 4, 28, 28)) {
        AddMinutesToTime(&g_settingsDraft.nightStartHour, &g_settingsDraft.nightStartMinute, 30);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (!useSystem && PtInUiBox(pt, 390, layout.switchDayY - 4, 28, 28)) {
        AddMinutesToTime(&g_settingsDraft.dayStartHour, &g_settingsDraft.dayStartMinute, -30);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (!useSystem && PtInUiBox(pt, 522, layout.switchDayY - 4, 28, 28)) {
        AddMinutesToTime(&g_settingsDraft.dayStartHour, &g_settingsDraft.dayStartMinute, 30);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    if (PtInUiBox(pt, 356, layout.footerY, 76, 34)) {
        ApplySettingsDraft(hwnd, true);
        return;
    }
    if (PtInUiBox(pt, 446, layout.footerY, 76, 34)) {
        ApplySettingsDraft(hwnd, false);
        return;
    }
    if (PtInUiBox(pt, 536, layout.footerY, 76, 34)) {
        DestroyWindow(hwnd);
        return;
    }
}

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        EnsureUiResources();
        LoadConfig();
        g_settingsDraft = g_config;
        if (!CanFollowWindowsNightLight()) {
            g_settingsDraft.followNightLight = false;
        }
        g_settingsDraftActive = true;
        g_hoverControl = HoverNone;
        g_trackingSettingsMouse = false;
        ApplyModernWindowFrame(hwnd);
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON))));
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON))));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps = {};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client = {};
        GetClientRect(hwnd, &client);
        HDC memDc = CreateCompatibleDC(dc);
        HBITMAP memBitmap = CreateCompatibleBitmap(dc, client.right - client.left, client.bottom - client.top);
        HGDIOBJ oldBitmap = SelectObject(memDc, memBitmap);
        DrawSettingsChrome(hwnd, memDc);
        BitBlt(dc, 0, 0, client.right - client.left, client.bottom - client.top, memDc, 0, 0, SRCCOPY);
        SelectObject(memDc, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDc);
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
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        UpdateSettingsHover(hwnd, pt);
        HandleSettingsClick(hwnd, pt);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (g_draggingBrightnessId != 0 && (wParam & MK_LBUTTON)) {
            int dragHover = g_draggingBrightnessId == kIdDayBrightness ? HoverDaySlider : HoverNightSlider;
            if (g_hoverControl != dragHover) {
                g_hoverControl = dragHover;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            SetDraftBrightnessFromPoint(hwnd, pt, g_draggingBrightnessId);
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return 0;
        }
        UpdateSettingsHover(hwnd, pt);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_trackingSettingsMouse = false;
        if (g_hoverControl != HoverNone) {
            g_hoverControl = HoverNone;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && g_hoverControl != HoverNone) {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_LBUTTONUP:
        if (g_draggingBrightnessId != 0) {
            g_draggingBrightnessId = 0;
            ReleaseCapture();
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
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_SETTINGCHANGE:
        ReloadUiTheme();
        EnsureUiResources();
        ApplyModernWindowFrame(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_settingsWindow = NULL;
        g_settingsDraftActive = false;
        g_draggingBrightnessId = 0;
        g_hoverControl = HoverNone;
        g_trackingSettingsMouse = false;
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ShowSettingsWindow(HWND owner) {
    if (g_settingsWindow) {
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
    EnsureUiResources();
    wc.hbrBackground = g_windowBrush;
    wc.lpszClassName = L"HdrSdrBrightnessSettingsWindow";
    RegisterClassExW(&wc);

    DWORD style = WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    DWORD exStyle = WS_EX_DLGMODALFRAME;
    LoadConfig();
    Config previewConfig = g_config;
    bool canFollow = CanFollowWindowsNightLight();
    if (!canFollow) {
        previewConfig.followNightLight = false;
    }
    SettingsLayout layout = BuildSettingsLayout(previewConfig.followNightLight && canFollow);
    SIZE windowSize = WindowSizeForClient(style, exStyle, Ui(kSettingsClientWidth), Ui(layout.clientHeight));

    g_settingsWindow = CreateWindowExW(exStyle, wc.lpszClassName, T(TxtSettingsTitle),
                                       style,
                                       CW_USEDEFAULT, CW_USEDEFAULT, windowSize.cx, windowSize.cy,
                                       owner, NULL, g_instance, NULL);
    CenterWindow(g_settingsWindow, windowSize.cx, windowSize.cy);
    ShowWindow(g_settingsWindow, SW_SHOW);
    UpdateWindow(g_settingsWindow);
}

DWORD WINAPI RegistryThreadProc(LPVOID) {
    HKEY cloudKey = NULL;
    HKEY appKey = NULL;
    RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\CloudStore\\Store", 0, KEY_NOTIFY, &cloudKey);
    RegCreateKeyExW(HKEY_CURRENT_USER, kConfigKey, 0, NULL, 0, KEY_NOTIFY, NULL, &appKey, NULL);

    HANDLE cloudEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    HANDLE appEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    HANDLE events[3] = {g_stopEvent, cloudEvent, appEvent};

    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        if (cloudKey) {
            ResetEvent(cloudEvent);
            RegNotifyChangeKeyValue(cloudKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, cloudEvent, TRUE);
        }
        if (appKey) {
            ResetEvent(appEvent);
            RegNotifyChangeKeyValue(appKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, appEvent, TRUE);
        }

        DWORD wait = WaitForMultipleObjects(3, events, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) break;
        if (g_mainWindow) PostMessageW(g_mainWindow, kRegistryChangedMessage, 0, 0);
    }

    if (cloudEvent) CloseHandle(cloudEvent);
    if (appEvent) CloseHandle(appEvent);
    if (cloudKey) RegCloseKey(cloudKey);
    if (appKey) RegCloseKey(appKey);
    return 0;
}

void StartRegistryThread() {
    g_stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_stopEvent) return;
    g_registryThread = CreateThread(NULL, 0, RegistryThreadProc, NULL, 0, NULL);
}

void StopRegistryThread() {
    if (g_stopEvent) SetEvent(g_stopEvent);
    if (g_registryThread) {
        WaitForSingleObject(g_registryThread, 3000);
        CloseHandle(g_registryThread);
        g_registryThread = NULL;
    }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = NULL;
    }
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
        StartRegistryThread();
        SetTimer(hwnd, kRecheckTimer, kRecheckMs, NULL);
        PostMessageW(hwnd, kApplyMessage, TRUE, 0);
        return 0;
    case kApplyMessage:
        ApplyCurrentBrightness(wParam != 0);
        return 0;
    case kRegistryChangedMessage:
        InvalidateNightLightScheduleCache();
        PostMessageW(hwnd, kApplyMessage, FALSE, 0);
        return 0;
    case WM_TIMER:
        if (wParam == kRecheckTimer) {
            ApplyCurrentBrightness(false);
            return 0;
        }
        if (wParam == kTransitionTimer) {
            ContinueBrightnessTransition();
            return 0;
        }
        break;
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
        if (message == WM_SETTINGCHANGE) {
            ReloadUiTheme();
            EnsureUiResources();
            if (g_settingsWindow) {
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
            g_config.startWithWindows = !IsStartupEnabled();
            SaveConfig();
            return 0;
        case kMenuDisplaySettings:
            ShellExecuteW(hwnd, L"open", L"ms-settings:display", NULL, NULL, SW_SHOWNORMAL);
            return 0;
        case kMenuNightLightSettings:
            ShellExecuteW(hwnd, L"open", L"ms-settings:nightlight", NULL, NULL, SW_SHOWNORMAL);
            return 0;
        case kMenuExit:
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_DESTROY:
        KillTimer(hwnd, kRecheckTimer);
        StopRegistryThread();
        RemoveTrayIcon();
        CleanupUiResources();
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
    g_instance = instance;
    SetProcessAppUserModelId();
    bool openSettingsOnLaunch = ShouldOpenSettingsOnLaunch();
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusInput, NULL);

    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\OledHdrSdrSyncMutex");
    if (!mutex) {
        if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (openSettingsOnLaunch) {
            ShowSettingsInExistingInstance();
        }
        CloseHandle(mutex);
        if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
        return 0;
    }

    LoadConfig();
    if (g_config.startWithWindows) {
        SetStartupEnabled(true);
    }
    if (!CreateMainWindow()) {
        CloseHandle(mutex);
        if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    if (openSettingsOnLaunch) {
        PostMessageW(g_mainWindow, WM_COMMAND, MAKEWPARAM(kMenuSettings, 0), 0);
    }

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (g_settingsWindow && IsDialogMessageW(g_settingsWindow, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(mutex);
    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
    return 0;
}
