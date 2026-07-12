#include "capture_request_queue.h"

namespace capture_request {

RequestDecision Queue::Request() {
    ++latestGeneration_;
    if (activeGeneration_ != 0) {
        pendingGeneration_ = latestGeneration_;
        return RequestDecision{latestGeneration_, false};
    }
    activeGeneration_ = latestGeneration_;
    pendingGeneration_ = 0;
    return RequestDecision{activeGeneration_, true};
}

void Queue::Cancel() {
    ++latestGeneration_;
    pendingGeneration_ = 0;
}

CompletionDecision Queue::Complete(std::uint64_t generation) {
    if (generation == 0 || generation != activeGeneration_) {
        return CompletionDecision{};
    }
    if (pendingGeneration_ != 0) {
        activeGeneration_ = pendingGeneration_;
        pendingGeneration_ = 0;
        return CompletionDecision{CompletionAction::StartLatest, activeGeneration_};
    }
    activeGeneration_ = 0;
    if (latestGeneration_ != generation) {
        return CompletionDecision{};
    }
    return CompletionDecision{CompletionAction::ShowResult, generation};
}

}  // namespace capture_request
