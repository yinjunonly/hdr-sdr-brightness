#pragma once

#include <windows.h>

namespace registry_watcher {

struct Watcher {
    HANDLE stopEvent;
    HANDLE thread;
    void* context;

    Watcher() : stopEvent(NULL), thread(NULL), context(NULL) {}
};

bool Start(Watcher* watcher, HWND notifyWindow, UINT notifyMessage, const wchar_t* appConfigKey);
void Stop(Watcher* watcher);

}  // namespace registry_watcher
