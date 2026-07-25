#include "store_startup_policy.h"

namespace store_startup_policy {

bool SetEnabled(Backend& backend, bool enabled) {
    return backend.SetStandardEnabled(enabled);
}

}  // namespace store_startup_policy
