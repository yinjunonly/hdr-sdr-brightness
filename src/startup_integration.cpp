#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601

#include "startup_integration.h"

#include <objbase.h>
#include <cwchar>
#include <string>
#include <vector>

#include "process_util.h"
#include "registry_util.h"
#include "store_startup_policy.h"

namespace startup_integration {
namespace {

const wchar_t kAppName[] = L"HdrSdrBrightness";
const wchar_t kLegacySyncAppName[] = L"HdrSdrSync";
const wchar_t kLegacyOledAppName[] = L"OledHdrSdrSync";
const wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t kStartupTaskName[] = L"HdrSdrBrightness";
const wchar_t kStoreStartupTaskId[] = L"HdrSdrBrightnessStartup";
const wchar_t kStoreFastStartupTaskName[] = L"HdrSdrBrightnessStoreFastStartup";
const wchar_t kStoreExecutionAliasName[] = L"HdrSdrBrightnessStore.exe";
const wchar_t kStoreFastStartupArguments[] = L"--background --store-fast-startup";

enum StoreStartupTaskState {
    StoreStartupTaskDisabled = 0,
    StoreStartupTaskDisabledByUser = 1,
    StoreStartupTaskEnabled = 2,
    StoreStartupTaskDisabledByPolicy = 3,
    StoreStartupTaskEnabledByPolicy = 4
};

enum StoreAsyncStatus {
    StoreAsyncStarted = 0,
    StoreAsyncCompleted = 1,
    StoreAsyncCanceled = 2,
    StoreAsyncError = 3
};

typedef void* StoreHString;
typedef int StoreTrustLevel;
typedef HRESULT(WINAPI* RoInitializeFn)(int);
typedef void(WINAPI* RoUninitializeFn)();
typedef HRESULT(WINAPI* RoGetActivationFactoryFn)(StoreHString, REFIID, void**);
typedef HRESULT(WINAPI* WindowsCreateStringFn)(PCWSTR, UINT32, StoreHString*);
typedef HRESULT(WINAPI* WindowsDeleteStringFn)(StoreHString);
typedef LONG(WINAPI* GetCurrentPackageFamilyNameFn)(UINT32*, PWSTR);

struct StoreIStartupTask;
struct StoreIStartupTaskStatics;
struct StoreIStoreContext;
struct StoreIStoreContextStatics;
struct StoreIStoreAppLicense;
struct StoreIAsyncInfo;
struct StoreIAsyncOperationStartupTask;
struct StoreIAsyncOperationStartupTaskState;
struct StoreIAsyncOperationStoreAppLicense;

struct StoreIStartupTaskVtbl {
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(StoreIStartupTask*, REFIID, void**);
    ULONG(STDMETHODCALLTYPE* AddRef)(StoreIStartupTask*);
    ULONG(STDMETHODCALLTYPE* Release)(StoreIStartupTask*);
    HRESULT(STDMETHODCALLTYPE* GetIids)(StoreIStartupTask*, ULONG*, IID**);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(StoreIStartupTask*, StoreHString*);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(StoreIStartupTask*, StoreTrustLevel*);
    HRESULT(STDMETHODCALLTYPE* RequestEnableAsync)(StoreIStartupTask*, StoreIAsyncOperationStartupTaskState**);
    HRESULT(STDMETHODCALLTYPE* Disable)(StoreIStartupTask*);
    HRESULT(STDMETHODCALLTYPE* get_State)(StoreIStartupTask*, int*);
    HRESULT(STDMETHODCALLTYPE* get_TaskId)(StoreIStartupTask*, StoreHString*);
};

struct StoreIStartupTask {
    const StoreIStartupTaskVtbl* lpVtbl;
};

struct StoreIStartupTaskStaticsVtbl {
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(StoreIStartupTaskStatics*, REFIID, void**);
    ULONG(STDMETHODCALLTYPE* AddRef)(StoreIStartupTaskStatics*);
    ULONG(STDMETHODCALLTYPE* Release)(StoreIStartupTaskStatics*);
    HRESULT(STDMETHODCALLTYPE* GetIids)(StoreIStartupTaskStatics*, ULONG*, IID**);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(StoreIStartupTaskStatics*, StoreHString*);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(StoreIStartupTaskStatics*, StoreTrustLevel*);
    HRESULT(STDMETHODCALLTYPE* GetForCurrentPackageAsync)(StoreIStartupTaskStatics*, void**);
    HRESULT(STDMETHODCALLTYPE* GetAsync)(StoreIStartupTaskStatics*, StoreHString, StoreIAsyncOperationStartupTask**);
};

struct StoreIStartupTaskStatics {
    const StoreIStartupTaskStaticsVtbl* lpVtbl;
};

struct StoreIStoreContextVtbl {
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(StoreIStoreContext*, REFIID, void**);
    ULONG(STDMETHODCALLTYPE* AddRef)(StoreIStoreContext*);
    ULONG(STDMETHODCALLTYPE* Release)(StoreIStoreContext*);
    HRESULT(STDMETHODCALLTYPE* GetIids)(StoreIStoreContext*, ULONG*, IID**);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(StoreIStoreContext*, StoreHString*);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(StoreIStoreContext*, StoreTrustLevel*);
    HRESULT(STDMETHODCALLTYPE* get_User)(StoreIStoreContext*, void**);
    HRESULT(STDMETHODCALLTYPE* add_OfflineLicensesChanged)(StoreIStoreContext*, void*, INT64*);
    HRESULT(STDMETHODCALLTYPE* remove_OfflineLicensesChanged)(StoreIStoreContext*, INT64);
    HRESULT(STDMETHODCALLTYPE* GetCustomerPurchaseIdAsync)(StoreIStoreContext*, StoreHString, StoreHString, void**);
    HRESULT(STDMETHODCALLTYPE* GetCustomerCollectionsIdAsync)(StoreIStoreContext*, StoreHString, StoreHString, void**);
    HRESULT(STDMETHODCALLTYPE* GetAppLicenseAsync)(StoreIStoreContext*, StoreIAsyncOperationStoreAppLicense**);
};

struct StoreIStoreContext {
    const StoreIStoreContextVtbl* lpVtbl;
};

struct StoreIStoreContextStaticsVtbl {
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(StoreIStoreContextStatics*, REFIID, void**);
    ULONG(STDMETHODCALLTYPE* AddRef)(StoreIStoreContextStatics*);
    ULONG(STDMETHODCALLTYPE* Release)(StoreIStoreContextStatics*);
    HRESULT(STDMETHODCALLTYPE* GetIids)(StoreIStoreContextStatics*, ULONG*, IID**);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(StoreIStoreContextStatics*, StoreHString*);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(StoreIStoreContextStatics*, StoreTrustLevel*);
    HRESULT(STDMETHODCALLTYPE* GetDefault)(StoreIStoreContextStatics*, StoreIStoreContext**);
    HRESULT(STDMETHODCALLTYPE* GetForUser)(StoreIStoreContextStatics*, void*, StoreIStoreContext**);
};

struct StoreIStoreContextStatics {
    const StoreIStoreContextStaticsVtbl* lpVtbl;
};

struct StoreIStoreAppLicenseVtbl {
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(StoreIStoreAppLicense*, REFIID, void**);
    ULONG(STDMETHODCALLTYPE* AddRef)(StoreIStoreAppLicense*);
    ULONG(STDMETHODCALLTYPE* Release)(StoreIStoreAppLicense*);
    HRESULT(STDMETHODCALLTYPE* GetIids)(StoreIStoreAppLicense*, ULONG*, IID**);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(StoreIStoreAppLicense*, StoreHString*);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(StoreIStoreAppLicense*, StoreTrustLevel*);
    HRESULT(STDMETHODCALLTYPE* get_SkuStoreId)(StoreIStoreAppLicense*, StoreHString*);
    HRESULT(STDMETHODCALLTYPE* get_IsActive)(StoreIStoreAppLicense*, BYTE*);
};

struct StoreIStoreAppLicense {
    const StoreIStoreAppLicenseVtbl* lpVtbl;
};

struct StoreIAsyncInfoVtbl {
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(StoreIAsyncInfo*, REFIID, void**);
    ULONG(STDMETHODCALLTYPE* AddRef)(StoreIAsyncInfo*);
    ULONG(STDMETHODCALLTYPE* Release)(StoreIAsyncInfo*);
    HRESULT(STDMETHODCALLTYPE* GetIids)(StoreIAsyncInfo*, ULONG*, IID**);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(StoreIAsyncInfo*, StoreHString*);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(StoreIAsyncInfo*, StoreTrustLevel*);
    HRESULT(STDMETHODCALLTYPE* get_Id)(StoreIAsyncInfo*, UINT32*);
    HRESULT(STDMETHODCALLTYPE* get_Status)(StoreIAsyncInfo*, int*);
    HRESULT(STDMETHODCALLTYPE* get_ErrorCode)(StoreIAsyncInfo*, HRESULT*);
    HRESULT(STDMETHODCALLTYPE* Cancel)(StoreIAsyncInfo*);
    HRESULT(STDMETHODCALLTYPE* Close)(StoreIAsyncInfo*);
};

struct StoreIAsyncInfo {
    const StoreIAsyncInfoVtbl* lpVtbl;
};

struct StoreIAsyncOperationStartupTaskVtbl {
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(StoreIAsyncOperationStartupTask*, REFIID, void**);
    ULONG(STDMETHODCALLTYPE* AddRef)(StoreIAsyncOperationStartupTask*);
    ULONG(STDMETHODCALLTYPE* Release)(StoreIAsyncOperationStartupTask*);
    HRESULT(STDMETHODCALLTYPE* GetIids)(StoreIAsyncOperationStartupTask*, ULONG*, IID**);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(StoreIAsyncOperationStartupTask*, StoreHString*);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(StoreIAsyncOperationStartupTask*, StoreTrustLevel*);
    HRESULT(STDMETHODCALLTYPE* put_Completed)(StoreIAsyncOperationStartupTask*, void*);
    HRESULT(STDMETHODCALLTYPE* get_Completed)(StoreIAsyncOperationStartupTask*, void**);
    HRESULT(STDMETHODCALLTYPE* GetResults)(StoreIAsyncOperationStartupTask*, StoreIStartupTask**);
};

struct StoreIAsyncOperationStartupTask {
    const StoreIAsyncOperationStartupTaskVtbl* lpVtbl;
};

struct StoreIAsyncOperationStartupTaskStateVtbl {
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(StoreIAsyncOperationStartupTaskState*, REFIID, void**);
    ULONG(STDMETHODCALLTYPE* AddRef)(StoreIAsyncOperationStartupTaskState*);
    ULONG(STDMETHODCALLTYPE* Release)(StoreIAsyncOperationStartupTaskState*);
    HRESULT(STDMETHODCALLTYPE* GetIids)(StoreIAsyncOperationStartupTaskState*, ULONG*, IID**);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(StoreIAsyncOperationStartupTaskState*, StoreHString*);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(StoreIAsyncOperationStartupTaskState*, StoreTrustLevel*);
    HRESULT(STDMETHODCALLTYPE* put_Completed)(StoreIAsyncOperationStartupTaskState*, void*);
    HRESULT(STDMETHODCALLTYPE* get_Completed)(StoreIAsyncOperationStartupTaskState*, void**);
    HRESULT(STDMETHODCALLTYPE* GetResults)(StoreIAsyncOperationStartupTaskState*, int*);
};

struct StoreIAsyncOperationStartupTaskState {
    const StoreIAsyncOperationStartupTaskStateVtbl* lpVtbl;
};

struct StoreIAsyncOperationStoreAppLicenseVtbl {
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(StoreIAsyncOperationStoreAppLicense*, REFIID, void**);
    ULONG(STDMETHODCALLTYPE* AddRef)(StoreIAsyncOperationStoreAppLicense*);
    ULONG(STDMETHODCALLTYPE* Release)(StoreIAsyncOperationStoreAppLicense*);
    HRESULT(STDMETHODCALLTYPE* GetIids)(StoreIAsyncOperationStoreAppLicense*, ULONG*, IID**);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(StoreIAsyncOperationStoreAppLicense*, StoreHString*);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(StoreIAsyncOperationStoreAppLicense*, StoreTrustLevel*);
    HRESULT(STDMETHODCALLTYPE* put_Completed)(StoreIAsyncOperationStoreAppLicense*, void*);
    HRESULT(STDMETHODCALLTYPE* get_Completed)(StoreIAsyncOperationStoreAppLicense*, void**);
    HRESULT(STDMETHODCALLTYPE* GetResults)(StoreIAsyncOperationStoreAppLicense*, StoreIStoreAppLicense**);
};

struct StoreIAsyncOperationStoreAppLicense {
    const StoreIAsyncOperationStoreAppLicenseVtbl* lpVtbl;
};

struct StoreWinRtApi {
    HMODULE module;
    RoInitializeFn roInitialize;
    RoUninitializeFn roUninitialize;
    RoGetActivationFactoryFn roGetActivationFactory;
    WindowsCreateStringFn createString;
    WindowsDeleteStringFn deleteString;
    bool initialized;

    StoreWinRtApi()
        : module(NULL),
          roInitialize(NULL),
          roUninitialize(NULL),
          roGetActivationFactory(NULL),
          createString(NULL),
          deleteString(NULL),
          initialized(false) {}
};

const GUID kIidStoreIStartupTaskStatics =
    {0xee5b60bd, 0xa148, 0x41a7, {0xb2, 0x6e, 0xe8, 0xb8, 0x8a, 0x1e, 0x62, 0xf8}};
const GUID kIidStoreIAsyncInfo =
    {0x00000036, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID kIidStoreIStoreContextStatics =
    {0x9c06ee5f, 0x15c0, 0x4e72, {0x93, 0x30, 0xd6, 0x19, 0x1c, 0xeb, 0xd1, 0x9c}};

bool LoadStoreWinRtApi(StoreWinRtApi* api) {
    if (!api) return false;
    api->module = LoadLibraryW(L"combase.dll");
    if (!api->module) return false;

    api->roInitialize = reinterpret_cast<RoInitializeFn>(GetProcAddress(api->module, "RoInitialize"));
    api->roUninitialize = reinterpret_cast<RoUninitializeFn>(GetProcAddress(api->module, "RoUninitialize"));
    api->roGetActivationFactory = reinterpret_cast<RoGetActivationFactoryFn>(GetProcAddress(api->module, "RoGetActivationFactory"));
    api->createString = reinterpret_cast<WindowsCreateStringFn>(GetProcAddress(api->module, "WindowsCreateString"));
    api->deleteString = reinterpret_cast<WindowsDeleteStringFn>(GetProcAddress(api->module, "WindowsDeleteString"));

    if (!api->roInitialize || !api->roUninitialize || !api->roGetActivationFactory ||
        !api->createString || !api->deleteString) {
        FreeLibrary(api->module);
        api->module = NULL;
        return false;
    }

    HRESULT hr = api->roInitialize(0);
    api->initialized = SUCCEEDED(hr);
    if (hr == RPC_E_CHANGED_MODE) {
        api->initialized = false;
        return true;
    }
    if (FAILED(hr)) {
        FreeLibrary(api->module);
        api->module = NULL;
        return false;
    }
    return true;
}

void UnloadStoreWinRtApi(StoreWinRtApi* api) {
    if (!api) return;
    if (api->initialized && api->roUninitialize) api->roUninitialize();
    if (api->module) FreeLibrary(api->module);
}

bool StoreCreateHString(StoreWinRtApi* api, const wchar_t* text, StoreHString* value) {
    if (!api || !api->createString || !text || !value) return false;
    *value = NULL;
    return SUCCEEDED(api->createString(text, static_cast<UINT32>(wcslen(text)), value));
}

void StoreDeleteHString(StoreWinRtApi* api, StoreHString value) {
    if (api && api->deleteString && value) api->deleteString(value);
}

bool WaitStoreAsync(void* operation, DWORD timeoutMs) {
    if (!operation) return false;

    StoreIAsyncInfo* info = NULL;
    HRESULT hr = reinterpret_cast<IUnknown*>(operation)->QueryInterface(kIidStoreIAsyncInfo,
                                                                        reinterpret_cast<void**>(&info));
    if (FAILED(hr) || !info) return false;

    DWORD startTick = GetTickCount();
    bool ok = false;
    for (;;) {
        int status = StoreAsyncStarted;
        hr = info->lpVtbl->get_Status(info, &status);
        if (FAILED(hr)) break;

        if (status == StoreAsyncCompleted) {
            ok = true;
            break;
        }
        if (status == StoreAsyncCanceled || status == StoreAsyncError) {
            break;
        }
        if (GetTickCount() - startTick > timeoutMs) {
            info->lpVtbl->Cancel(info);
            break;
        }
        Sleep(25);
    }

    info->lpVtbl->Close(info);
    info->lpVtbl->Release(info);
    return ok;
}

bool GetCurrentPackageFamilyNameValue(std::wstring* packageFamilyName) {
    if (!packageFamilyName) return false;

    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    if (!kernel) return false;

    GetCurrentPackageFamilyNameFn getCurrentPackageFamilyName =
        reinterpret_cast<GetCurrentPackageFamilyNameFn>(
            GetProcAddress(kernel, "GetCurrentPackageFamilyName"));
    if (!getCurrentPackageFamilyName) return false;

    UINT32 length = 0;
    LONG rc = getCurrentPackageFamilyName(&length, NULL);
    if (rc != ERROR_INSUFFICIENT_BUFFER || length == 0) return false;

    std::vector<wchar_t> buffer(length, L'\0');
    rc = getCurrentPackageFamilyName(&length, buffer.data());
    if (rc != ERROR_SUCCESS || length == 0 || buffer[0] == L'\0') return false;

    *packageFamilyName = buffer.data();
    return true;
}

bool TryReadStoreStartupStateFromRegistry(int* state) {
    if (!state) return false;

    std::wstring packageFamilyName;
    if (!GetCurrentPackageFamilyNameValue(&packageFamilyName)) return false;

    std::wstring keyPath =
        L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\"
        L"AppModel\\SystemAppData\\" +
        packageFamilyName + L"\\" + kStoreStartupTaskId;

    DWORD value = 0;
    if (!ReadDwordValue(HKEY_CURRENT_USER, keyPath.c_str(), L"State", &value)) return false;

    *state = static_cast<int>(value);
    return true;
}

bool GetStoreStartupTask(StoreWinRtApi* api, StoreIStartupTask** task) {
    if (!api || !task) return false;
    *task = NULL;

    StoreHString className = NULL;
    StoreHString taskId = NULL;
    StoreIStartupTaskStatics* statics = NULL;
    StoreIAsyncOperationStartupTask* operation = NULL;

    bool ok = false;
    HRESULT hr = E_FAIL;
    if (!StoreCreateHString(api, L"Windows.ApplicationModel.StartupTask", &className)) goto cleanup;
    hr = api->roGetActivationFactory(className, kIidStoreIStartupTaskStatics,
                                     reinterpret_cast<void**>(&statics));
    if (FAILED(hr) || !statics) goto cleanup;
    if (!StoreCreateHString(api, kStoreStartupTaskId, &taskId)) goto cleanup;

    hr = statics->lpVtbl->GetAsync(statics, taskId, &operation);
    if (FAILED(hr) || !operation) goto cleanup;
    if (!WaitStoreAsync(operation, 3000)) goto cleanup;

    hr = operation->lpVtbl->GetResults(operation, task);
    ok = SUCCEEDED(hr) && *task != NULL;

cleanup:
    if (operation) operation->lpVtbl->Release(operation);
    if (statics) statics->lpVtbl->Release(statics);
    StoreDeleteHString(api, taskId);
    StoreDeleteHString(api, className);
    return ok;
}

bool TryReadStoreAppLicenseActiveWithApi(StoreWinRtApi* api, bool* active) {
    if (!api || !active) return false;
    *active = true;

    StoreHString className = NULL;
    StoreIStoreContextStatics* statics = NULL;
    StoreIStoreContext* context = NULL;
    StoreIAsyncOperationStoreAppLicense* operation = NULL;
    StoreIStoreAppLicense* license = NULL;

    bool ok = false;
    BYTE value = 0;
    HRESULT hr = E_FAIL;
    if (!StoreCreateHString(api, L"Windows.Services.Store.StoreContext", &className)) goto cleanup;
    hr = api->roGetActivationFactory(className, kIidStoreIStoreContextStatics,
                                     reinterpret_cast<void**>(&statics));
    if (FAILED(hr) || !statics) goto cleanup;

    hr = statics->lpVtbl->GetDefault(statics, &context);
    if (FAILED(hr) || !context) goto cleanup;

    hr = context->lpVtbl->GetAppLicenseAsync(context, &operation);
    if (FAILED(hr) || !operation) goto cleanup;
    if (!WaitStoreAsync(operation, 6000)) goto cleanup;

    hr = operation->lpVtbl->GetResults(operation, &license);
    if (FAILED(hr) || !license) goto cleanup;

    hr = license->lpVtbl->get_IsActive(license, &value);
    if (FAILED(hr)) goto cleanup;

    *active = value != 0;
    ok = true;

cleanup:
    if (license) license->lpVtbl->Release(license);
    if (operation) operation->lpVtbl->Release(operation);
    if (context) context->lpVtbl->Release(context);
    if (statics) statics->lpVtbl->Release(statics);
    StoreDeleteHString(api, className);
    return ok;
}

bool IsRunKeyStartupEnabled() {
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

void SetRunKeyStartupEnabled(bool enabled) {
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

bool IsScheduledTaskEnabled(const std::wstring& taskName) {
    std::wstring command = L"schtasks.exe /Query /TN " + QuoteCommandLineArgument(taskName);
    return RunHiddenCommand(command, 3000);
}

bool DeleteScheduledTask(const std::wstring& taskName) {
    if (!IsScheduledTaskEnabled(taskName)) return true;
    std::wstring command = L"schtasks.exe /Delete /TN " + QuoteCommandLineArgument(taskName) + L" /F";
    if (RunHiddenCommand(command, 5000)) return true;

    std::wstring script =
        L"Unregister-ScheduledTask -TaskName " + QuotePowerShellString(taskName) +
        L" -Confirm:$false -ErrorAction SilentlyContinue";
    std::wstring powershell =
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command " +
        QuoteCommandLineArgument(script);
    return RunHiddenCommand(powershell, 10000);
}

bool CreateLogonScheduledTask(const std::wstring& taskName,
                              const std::wstring& executable,
                              const std::wstring& arguments) {
    if (taskName.empty() || executable.empty()) return false;
    std::wstring action = QuoteCommandLineArgument(executable);
    if (!arguments.empty()) action += L" " + arguments;
    std::wstring command =
        L"schtasks.exe /Create /TN " + QuoteCommandLineArgument(taskName) +
        L" /SC ONLOGON /TR " + QuoteCommandLineArgument(action) +
        L" /RL LIMITED /F";
    if (RunHiddenCommand(command, 5000)) return true;

    std::wstring script =
        L"$ErrorActionPreference='Stop';"
        L"$user=[System.Security.Principal.WindowsIdentity]::GetCurrent().Name;"
        L"$action=New-ScheduledTaskAction -Execute " + QuotePowerShellString(executable) +
        L" -Argument " + QuotePowerShellString(arguments) + L";"
        L"$trigger=New-ScheduledTaskTrigger -AtLogOn -User $user;"
        L"$principal=New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited;"
        L"$settings=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries "
        L"-ExecutionTimeLimit (New-TimeSpan -Seconds 0);"
        L"Register-ScheduledTask -TaskName " + QuotePowerShellString(taskName) +
        L" -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Force | Out-Null";
    std::wstring powershell =
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command " +
        QuoteCommandLineArgument(script);
    return RunHiddenCommand(powershell, 15000);
}

bool IsScheduledTaskStartupEnabled() {
    return IsScheduledTaskEnabled(kStartupTaskName);
}

bool SetScheduledTaskStartupEnabled(bool enabled) {
    if (!enabled) return DeleteScheduledTask(kStartupTaskName);
    return CreateLogonScheduledTask(kStartupTaskName, GetExePath(), L"--background");
}

std::wstring GetStoreExecutionAliasPath() {
    DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", NULL, 0);
    if (required == 0) return L"";

    std::vector<wchar_t> buffer(required, L'\0');
    DWORD written = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), required);
    if (written == 0 || written >= required) return L"";

    return JoinPath(JoinPath(buffer.data(), L"Microsoft\\WindowsApps"),
                    kStoreExecutionAliasName);
}

bool IsStoreFastStartupEnabledInternal() {
    return IsScheduledTaskEnabled(kStoreFastStartupTaskName);
}

bool SetStoreFastStartupEnabledInternal(bool enabled) {
    if (!enabled) return DeleteScheduledTask(kStoreFastStartupTaskName);
    std::wstring aliasPath = GetStoreExecutionAliasPath();
    if (aliasPath.empty() || !FileExists(aliasPath)) return false;
    return CreateLogonScheduledTask(kStoreFastStartupTaskName, aliasPath,
                                    kStoreFastStartupArguments);
}

bool IsStoreStartupTaskEnabledInternal(bool preferRegistry) {
    int state = StoreStartupTaskDisabled;
    if (preferRegistry && TryReadStoreStartupStateFromRegistry(&state)) {
        return state == StoreStartupTaskEnabled || state == StoreStartupTaskEnabledByPolicy;
    }

    StoreWinRtApi api;
    bool apiLoaded = LoadStoreWinRtApi(&api);
    StoreIStartupTask* task = NULL;
    bool hasState = false;
    if (apiLoaded && GetStoreStartupTask(&api, &task)) {
        hasState = SUCCEEDED(task->lpVtbl->get_State(task, &state));
        task->lpVtbl->Release(task);
    }
    if (!hasState) hasState = TryReadStoreStartupStateFromRegistry(&state);
    if (apiLoaded) UnloadStoreWinRtApi(&api);
    return hasState &&
           (state == StoreStartupTaskEnabled || state == StoreStartupTaskEnabledByPolicy);
}

bool SetStoreStartupTaskEnabledInternal(bool enabled) {
    StoreWinRtApi api;
    if (!LoadStoreWinRtApi(&api)) return false;

    StoreIStartupTask* task = NULL;
    bool ok = false;
    if (!GetStoreStartupTask(&api, &task)) goto cleanup;

    if (!enabled) {
        ok = SUCCEEDED(task->lpVtbl->Disable(task));
        goto cleanup;
    }

    {
        StoreIAsyncOperationStartupTaskState* operation = NULL;
        HRESULT hr = task->lpVtbl->RequestEnableAsync(task, &operation);
        if (SUCCEEDED(hr) && operation && WaitStoreAsync(operation, 30000)) {
            int state = StoreStartupTaskDisabled;
            if (SUCCEEDED(operation->lpVtbl->GetResults(operation, &state))) {
                ok = state == StoreStartupTaskEnabled || state == StoreStartupTaskEnabledByPolicy;
            }
        }
        if (operation) operation->lpVtbl->Release(operation);
    }

cleanup:
    if (task) task->lpVtbl->Release(task);
    UnloadStoreWinRtApi(&api);
    return ok;
}

class StoreStartupBackend : public store_startup_policy::Backend {
public:
    explicit StoreStartupBackend(bool preferRegistry = false)
        : preferRegistry_(preferRegistry) {}

    bool IsStandardEnabled() override {
        return IsStoreStartupTaskEnabledInternal(preferRegistry_);
    }

    bool SetStandardEnabled(bool enabled) override {
        return SetStoreStartupTaskEnabledInternal(enabled);
    }

    bool IsFastEnabled() override {
        return IsStoreFastStartupEnabledInternal();
    }

    bool SetFastEnabled(bool enabled) override {
        return SetStoreFastStartupEnabledInternal(enabled);
    }

private:
    bool preferRegistry_;
};

}  // namespace

bool IsStoreStartupEnabled() {
    StoreStartupBackend backend;
    return backend.IsStandardEnabled();
}

bool SetStoreStartupEnabled(bool enabled) {
    StoreStartupBackend backend;
    return store_startup_policy::SetEnabled(backend, enabled);
}

void RepairStoreFastStartupIfNeeded() {
    StoreStartupBackend backend;
    store_startup_policy::Reconcile(backend);
}

bool ShouldRunStoreFastStartup() {
    StoreStartupBackend backend(true);
    return store_startup_policy::ShouldRunBackground(backend);
}

bool TryReadStoreAppLicenseActive(bool* active) {
    if (!active) return false;
    *active = true;

    StoreWinRtApi api;
    if (!LoadStoreWinRtApi(&api)) return false;
    bool ok = TryReadStoreAppLicenseActiveWithApi(&api, active);
    UnloadStoreWinRtApi(&api);
    return ok;
}

bool IsPortableStartupEnabled() {
    return IsRunKeyStartupEnabled() || IsScheduledTaskStartupEnabled();
}

bool SetPortableStartupEnabled(bool enabled) {
    if (enabled) {
        SetScheduledTaskStartupEnabled(true);
        SetRunKeyStartupEnabled(true);
    } else {
        SetScheduledTaskStartupEnabled(false);
        SetRunKeyStartupEnabled(false);
    }
    return IsPortableStartupEnabled() == enabled;
}

void RepairPortableScheduledTaskStartupIfNeeded(bool shouldStartWithWindows) {
    if (shouldStartWithWindows && !IsScheduledTaskStartupEnabled()) {
        SetScheduledTaskStartupEnabled(true);
    }
}

}  // namespace startup_integration
