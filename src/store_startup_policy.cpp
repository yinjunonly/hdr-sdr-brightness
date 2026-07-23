#include "store_startup_policy.h"

namespace store_startup_policy {

bool SetEnabled(Backend& backend, bool enabled) {
    bool standardOk = backend.SetStandardEnabled(enabled);
    if (enabled) {
        if (standardOk) backend.SetFastEnabled(true);
    } else {
        backend.SetFastEnabled(false);
    }
    return standardOk;
}

void Reconcile(Backend& backend) {
    bool standardEnabled = backend.IsStandardEnabled();
    bool fastEnabled = backend.IsFastEnabled();
    if (standardEnabled && !fastEnabled) {
        backend.SetFastEnabled(true);
    } else if (!standardEnabled && fastEnabled) {
        backend.SetFastEnabled(false);
    }
}

bool ShouldRunBackground(Backend& backend) {
    if (backend.IsStandardEnabled()) return true;
    if (backend.IsFastEnabled()) backend.SetFastEnabled(false);
    return false;
}

}  // namespace store_startup_policy
