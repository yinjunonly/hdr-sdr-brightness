#define WIN32_LEAN_AND_MEAN

#include "store_registry_event_bridge.h"

#include <sddl.h>
#include <wbemidl.h>

#include <new>
#include <string>
#include <vector>

namespace store_registry_event_bridge {
namespace {

bool GetCurrentUserSid(std::wstring* sid) {
    if (!sid) return false;

    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    DWORD bytes = 0;
    GetTokenInformation(token, TokenUser, NULL, 0, &bytes);
    if (bytes == 0) {
        CloseHandle(token);
        return false;
    }

    std::vector<BYTE> buffer(bytes);
    if (!GetTokenInformation(token, TokenUser, &buffer[0], bytes, &bytes)) {
        CloseHandle(token);
        return false;
    }
    CloseHandle(token);

    const TOKEN_USER* user =
        reinterpret_cast<const TOKEN_USER*>(&buffer[0]);
    LPWSTR sidText = NULL;
    if (!ConvertSidToStringSidW(user->User.Sid, &sidText)) {
        return false;
    }

    *sid = sidText;
    LocalFree(sidText);
    return true;
}

std::wstring EscapeWqlString(const std::wstring& value) {
    std::wstring escaped;
    escaped.reserve(value.size() * 2);
    for (size_t index = 0; index < value.size(); ++index) {
        const wchar_t character = value[index];
        if (character == L'\\' || character == L'\'') {
            escaped.push_back(L'\\');
        }
        escaped.push_back(character);
    }
    return escaped;
}

}  // namespace

struct Subscription::State {
    bool comInitialized;
    IWbemServices* services;
    IEnumWbemClassObject* events;

    State()
        : comInitialized(false),
          services(NULL),
          events(NULL) {}
};

Subscription::Subscription()
    : state_(NULL) {}

Subscription::~Subscription() {
    Stop();
}

bool Subscription::Start(const wchar_t* currentUserRelativePath) {
    Stop();
    if (!currentUserRelativePath || !*currentUserRelativePath) return false;

    State* state = new (std::nothrow) State();
    if (!state) return false;
    state_ = state;

    HRESULT result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (result != S_OK && result != S_FALSE) {
        Stop();
        return false;
    }
    state->comInitialized = true;

    result = CoInitializeSecurity(
        NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    if (FAILED(result) && result != RPC_E_TOO_LATE) {
        Stop();
        return false;
    }

    IWbemLocator* locator = NULL;
    result = CoCreateInstance(
        CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, reinterpret_cast<void**>(&locator));
    if (FAILED(result) || !locator) {
        Stop();
        return false;
    }

    BSTR nameSpace = SysAllocString(L"ROOT\\DEFAULT");
    if (!nameSpace) {
        locator->Release();
        Stop();
        return false;
    }
    result = locator->ConnectServer(
        nameSpace, NULL, NULL, NULL, 0, NULL, NULL, &state->services);
    SysFreeString(nameSpace);
    locator->Release();
    if (FAILED(result) || !state->services) {
        Stop();
        return false;
    }

    result = CoSetProxyBlanket(
        state->services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE);
    if (FAILED(result)) {
        Stop();
        return false;
    }

    std::wstring sid;
    if (!GetCurrentUserSid(&sid)) {
        Stop();
        return false;
    }

    std::wstring relativePath(currentUserRelativePath);
    while (!relativePath.empty() &&
           (relativePath[0] == L'\\' || relativePath[0] == L'/')) {
        relativePath.erase(0, 1);
    }
    const std::wstring rootPath =
        EscapeWqlString(sid + L"\\" + relativePath);
    const std::wstring query =
        L"SELECT * FROM RegistryTreeChangeEvent "
        L"WHERE Hive='HKEY_USERS' AND RootPath='" +
        rootPath + L"'";

    BSTR queryLanguage = SysAllocString(L"WQL");
    BSTR queryText = SysAllocString(query.c_str());
    if (!queryLanguage || !queryText) {
        if (queryLanguage) SysFreeString(queryLanguage);
        if (queryText) SysFreeString(queryText);
        Stop();
        return false;
    }

    result = state->services->ExecNotificationQuery(
        queryLanguage, queryText,
        WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY,
        NULL, &state->events);
    SysFreeString(queryLanguage);
    SysFreeString(queryText);
    if (FAILED(result) || !state->events) {
        Stop();
        return false;
    }

    result = CoSetProxyBlanket(
        state->events, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE);
    if (FAILED(result)) {
        Stop();
        return false;
    }

    return true;
}

bool Subscription::WaitForChange(DWORD timeoutMs) {
    if (!state_ || !state_->events) return false;

    IWbemClassObject* event = NULL;
    ULONG returned = 0;
    const HRESULT result =
        state_->events->Next(timeoutMs, 1, &event, &returned);
    if (event) event->Release();
    return result == WBEM_S_NO_ERROR && returned == 1;
}

void Subscription::Stop() {
    if (!state_) return;

    if (state_->events) {
        state_->events->Release();
        state_->events = NULL;
    }
    if (state_->services) {
        state_->services->Release();
        state_->services = NULL;
    }
    if (state_->comInitialized) {
        CoUninitialize();
        state_->comInitialized = false;
    }

    delete state_;
    state_ = NULL;
}

}  // namespace store_registry_event_bridge
