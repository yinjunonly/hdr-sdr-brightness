#include <cstdio>
#include <vector>

#include "store_startup_policy.h"

namespace {

class FakeBackend : public store_startup_policy::Backend {
public:
    bool standardEnabled = false;
    bool standardSetResult = true;
    std::vector<int> calls;

    bool IsStandardEnabled() override { return standardEnabled; }

    bool SetStandardEnabled(bool enabled) override {
        calls.push_back(enabled ? 1 : -1);
        if (standardSetResult) standardEnabled = enabled;
        return standardSetResult;
    }
};

bool Expect(bool condition, const char* message) {
    if (condition) return true;
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}

}  // namespace

int main() {
    {
        FakeBackend backend;

        bool ok = store_startup_policy::SetEnabled(backend, true);

        if (!Expect(ok, "standard Store startup success should keep the single toggle enabled")) return 1;
        if (!Expect(backend.standardEnabled, "standard Store startup should be enabled")) return 1;
        if (!Expect(backend.calls == std::vector<int>({1}),
                    "enabling must only request the Windows-managed standard startup task")) return 1;
    }

    {
        FakeBackend backend;
        backend.standardEnabled = true;

        bool ok = store_startup_policy::SetEnabled(backend, false);

        if (!Expect(ok, "disabling the single toggle should disable standard startup")) return 1;
        if (!Expect(!backend.standardEnabled,
                    "disabling should change the Windows-managed standard startup task")) return 1;
        if (!Expect(backend.calls == std::vector<int>({-1}),
                    "disabling must not manage a custom fast-startup task")) return 1;
    }

    std::puts("PASS: Store startup policy only manages the Windows startup task.");
    return 0;
}
