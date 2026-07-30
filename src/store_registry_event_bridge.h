#pragma once

#include <windows.h>

namespace store_registry_event_bridge {

class Subscription {
public:
    Subscription();
    ~Subscription();

    bool Start(const wchar_t* currentUserRelativePath);
    bool WaitForChange(DWORD timeoutMs);
    void Stop();

private:
    Subscription(const Subscription&);
    Subscription& operator=(const Subscription&);

    struct State;
    State* state_;
};

}  // namespace store_registry_event_bridge
