#include <windows.h>

#include <cstdio>

#include "registry_watcher.h"

int main() {
    registry_watcher::Watcher watcher;
    bool started = registry_watcher::Start(
        &watcher,
        NULL,
        WM_APP + 1,
        L"Software\\OledHdrSdrSync\\Tests\\NullNotifyWindow"
    );
    if (started) {
        registry_watcher::Stop(&watcher);
        std::fprintf(
            stderr,
            "FAIL: registry watcher accepted a null notification window.\n"
        );
        return 1;
    }

    std::printf("PASS: registry watcher rejects a null notification window.\n");
    return 0;
}
