#pragma once

namespace store_startup_policy {

class Backend {
public:
    virtual ~Backend() = default;

    virtual bool IsStandardEnabled() = 0;
    virtual bool SetStandardEnabled(bool enabled) = 0;
};

bool SetEnabled(Backend& backend, bool enabled);

}  // namespace store_startup_policy
