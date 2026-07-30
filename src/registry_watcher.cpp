#define WIN32_LEAN_AND_MEAN

#include "registry_watcher.h"

#ifdef HSB_STORE_BUILD
#include "store_registry_event_bridge.h"
#endif

#include <string>

namespace registry_watcher {
namespace {

struct WatchContext {
    HANDLE stopEvent;
    HWND notifyWindow;
    UINT notifyMessage;
    std::wstring appConfigKey;
};

DWORD WINAPI WatchThreadProc(LPVOID param) {
    WatchContext* context = static_cast<WatchContext*>(param);
    const wchar_t* cloudStorePath =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\CloudStore\\Store";
    HKEY cloudKey = NULL;
    HKEY appKey = NULL;
    RegOpenKeyExW(HKEY_CURRENT_USER, cloudStorePath,
                  0, KEY_NOTIFY, &cloudKey);
    RegCreateKeyExW(HKEY_CURRENT_USER, context->appConfigKey.c_str(), 0, NULL, 0, KEY_NOTIFY, NULL, &appKey, NULL);

#ifdef HSB_STORE_BUILD
    store_registry_event_bridge::Subscription storeCloudEvents;
    const bool storeCloudEventsStarted =
        storeCloudEvents.Start(cloudStorePath);
#endif

    HANDLE cloudEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    HANDLE appEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!cloudEvent || !appEvent) {
        if (cloudEvent) CloseHandle(cloudEvent);
        if (appEvent) CloseHandle(appEvent);
        if (cloudKey) RegCloseKey(cloudKey);
        if (appKey) RegCloseKey(appKey);
        return 0;
    }
    HANDLE events[3] = {context->stopEvent, cloudEvent, appEvent};

    while (WaitForSingleObject(context->stopEvent, 0) == WAIT_TIMEOUT) {
        if (cloudKey) {
            ResetEvent(cloudEvent);
            RegNotifyChangeKeyValue(cloudKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, cloudEvent, TRUE);
        }
        if (appKey) {
            ResetEvent(appEvent);
            RegNotifyChangeKeyValue(appKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, appEvent, TRUE);
        }

        bool changed = false;
        while (!changed) {
#ifdef HSB_STORE_BUILD
            DWORD wait = WaitForMultipleObjects(3, events, FALSE, 0);
            if (wait == WAIT_OBJECT_0) break;
            if (wait == WAIT_OBJECT_0 + 1 ||
                wait == WAIT_OBJECT_0 + 2) {
                changed = true;
                break;
            }
            if (storeCloudEventsStarted &&
                storeCloudEvents.WaitForChange(250)) {
                changed = true;
                break;
            }
            if (!storeCloudEventsStarted) {
                wait = WaitForMultipleObjects(3, events, FALSE, INFINITE);
                if (wait == WAIT_OBJECT_0) break;
                changed = wait == WAIT_OBJECT_0 + 1 ||
                          wait == WAIT_OBJECT_0 + 2;
            }
#else
            const DWORD wait =
                WaitForMultipleObjects(3, events, FALSE, INFINITE);
            if (wait == WAIT_OBJECT_0) break;
            changed = wait == WAIT_OBJECT_0 + 1 ||
                      wait == WAIT_OBJECT_0 + 2;
#endif
        }

        if (WaitForSingleObject(context->stopEvent, 0) != WAIT_TIMEOUT) {
            break;
        }
        if (changed && context->notifyWindow) {
            PostMessageW(
                context->notifyWindow, context->notifyMessage, 0, 0);
        }
    }

    if (cloudEvent) CloseHandle(cloudEvent);
    if (appEvent) CloseHandle(appEvent);
    if (cloudKey) RegCloseKey(cloudKey);
    if (appKey) RegCloseKey(appKey);
    return 0;
}

}  // namespace

bool Start(Watcher* watcher, HWND notifyWindow, UINT notifyMessage, const wchar_t* appConfigKey) {
    if (!watcher || watcher->thread || !notifyWindow) return false;

    HANDLE stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!stopEvent) return false;

    WatchContext* context = new WatchContext();
    context->stopEvent = stopEvent;
    context->notifyWindow = notifyWindow;
    context->notifyMessage = notifyMessage;
    context->appConfigKey = appConfigKey ? appConfigKey : L"";

    HANDLE thread = CreateThread(NULL, 0, WatchThreadProc, context, 0, NULL);
    if (!thread) {
        delete context;
        CloseHandle(stopEvent);
        return false;
    }

    watcher->stopEvent = stopEvent;
    watcher->thread = thread;
    watcher->context = context;
    return true;
}

void Stop(Watcher* watcher) {
    if (!watcher) return;

    if (watcher->stopEvent) SetEvent(watcher->stopEvent);
    if (watcher->thread) {
        DWORD wait = WaitForSingleObject(watcher->thread, 3000);
        CloseHandle(watcher->thread);
        watcher->thread = NULL;
        if (wait != WAIT_OBJECT_0) {
            return;
        }
    }

    if (watcher->stopEvent) {
        CloseHandle(watcher->stopEvent);
        watcher->stopEvent = NULL;
    }
    delete static_cast<WatchContext*>(watcher->context);
    watcher->context = NULL;
}

}  // namespace registry_watcher
