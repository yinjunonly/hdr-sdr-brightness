#include <cstdio>
#include <vector>

#include "store_startup_policy.h"

namespace {

class FakeBackend : public store_startup_policy::Backend {
public:
    bool standardEnabled = false;
    bool fastEnabled = false;
    bool standardSetResult = true;
    bool fastSetResult = true;
    std::vector<int> calls;

    bool IsStandardEnabled() override { return standardEnabled; }

    bool SetStandardEnabled(bool enabled) override {
        calls.push_back(enabled ? 1 : -1);
        if (standardSetResult) standardEnabled = enabled;
        return standardSetResult;
    }

    bool IsFastEnabled() override { return fastEnabled; }

    bool SetFastEnabled(bool enabled) override {
        calls.push_back(enabled ? 2 : -2);
        if (fastSetResult) fastEnabled = enabled;
        return fastSetResult;
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
        backend.fastSetResult = false;

        bool ok = store_startup_policy::SetEnabled(backend, true);

        if (!Expect(ok, "standard Store startup success should keep the single toggle enabled")) return 1;
        if (!Expect(backend.standardEnabled, "standard Store startup should be enabled")) return 1;
        if (!Expect(backend.calls == std::vector<int>({1, 2}),
                    "enabling should request standard startup before best-effort fast startup")) return 1;
    }

    {
        FakeBackend backend;
        backend.standardEnabled = true;

        store_startup_policy::Reconcile(backend);

        if (!Expect(backend.fastEnabled, "an enabled standard startup should repair a missing fast task")) return 1;
        if (!Expect(backend.calls == std::vector<int>({2}),
                    "repair should only create the missing fast task")) return 1;
    }

    {
        FakeBackend backend;
        backend.standardEnabled = true;
        backend.fastEnabled = true;

        bool ok = store_startup_policy::SetEnabled(backend, false);

        if (!Expect(ok, "disabling the single toggle should disable standard startup")) return 1;
        if (!Expect(!backend.standardEnabled && !backend.fastEnabled,
                    "disabling should remove both standard and fast startup")) return 1;
        if (!Expect(backend.calls == std::vector<int>({-1, -2}),
                    "disabling should remove standard startup before the fast task")) return 1;
    }

    {
        FakeBackend backend;
        backend.fastEnabled = true;

        bool shouldRun = store_startup_policy::ShouldRunBackground(backend);

        if (!Expect(!shouldRun, "Windows disabling standard startup must block fast background startup")) return 1;
        if (!Expect(!backend.fastEnabled, "a blocked fast startup should remove its stale task")) return 1;
        if (!Expect(backend.calls == std::vector<int>({-2}),
                    "a blocked fast startup should only disable the fast task")) return 1;
    }

    std::puts("PASS: Store startup policy enables, repairs, and respects Windows disable state.");
    return 0;
}
