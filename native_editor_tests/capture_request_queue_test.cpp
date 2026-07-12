#include "../src/capture_request_queue.h"

#include <cstdio>

namespace {

bool Expect(bool condition, const char* message) {
    if (condition) return true;
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}

}  // namespace

int main() {
    capture_request::Queue queue;

    capture_request::RequestDecision first = queue.Request();
    if (!Expect(first.startCapture && first.generation == 1,
                "the first request must start immediately.")) {
        return 1;
    }

    capture_request::RequestDecision second = queue.Request();
    capture_request::RequestDecision third = queue.Request();
    if (!Expect(!second.startCapture && second.generation == 2,
                "a second in-flight request must be coalesced.")) {
        return 1;
    }
    if (!Expect(!third.startCapture && third.generation == 3,
                "rapid requests must keep only the newest generation pending.")) {
        return 1;
    }

    capture_request::CompletionDecision stale = queue.Complete(first.generation);
    if (!Expect(stale.action == capture_request::CompletionAction::StartLatest &&
                    stale.generation == third.generation,
                "a stale completion must start exactly the latest pending capture.")) {
        return 1;
    }

    capture_request::CompletionDecision obsolete = queue.Complete(second.generation);
    if (!Expect(obsolete.action == capture_request::CompletionAction::Ignore,
                "a completion that is not active must be ignored.")) {
        return 1;
    }

    capture_request::CompletionDecision latest = queue.Complete(third.generation);
    if (!Expect(latest.action == capture_request::CompletionAction::ShowResult &&
                    latest.generation == third.generation,
                "only the latest completion may show an editor.")) {
        return 1;
    }

    capture_request::RequestDecision fourth = queue.Request();
    capture_request::CompletionDecision fourthDone = queue.Complete(fourth.generation);
    if (!Expect(fourth.startCapture && fourth.generation == 4 &&
                    fourthDone.action == capture_request::CompletionAction::ShowResult,
                "the queue must return to idle after the latest result is shown.")) {
        return 1;
    }

    capture_request::RequestDecision regionBeforeFullscreen = queue.Request();
    queue.Cancel();
    capture_request::CompletionDecision cancelled =
        queue.Complete(regionBeforeFullscreen.generation);
    if (!Expect(cancelled.action == capture_request::CompletionAction::Ignore,
                "a fullscreen request must suppress an older in-flight region result.")) {
        return 1;
    }

    capture_request::RequestDecision afterCancel = queue.Request();
    if (!Expect(afterCancel.startCapture,
                "the region queue must return to idle after a cancelled capture finishes.")) {
        return 1;
    }

    std::puts("PASS: repeated screenshot requests coalesce to the latest generation.");
    return 0;
}
