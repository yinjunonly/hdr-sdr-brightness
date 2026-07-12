#pragma once

#include <cstdint>

namespace capture_request {

enum class CompletionAction {
    Ignore,
    StartLatest,
    ShowResult,
};

struct RequestDecision {
    RequestDecision(std::uint64_t value = 0, bool start = false)
        : generation(value), startCapture(start) {}

    std::uint64_t generation;
    bool startCapture;
};

struct CompletionDecision {
    CompletionDecision(CompletionAction value = CompletionAction::Ignore,
                       std::uint64_t requestGeneration = 0)
        : action(value), generation(requestGeneration) {}

    CompletionAction action;
    std::uint64_t generation;
};

class Queue {
public:
    RequestDecision Request();
    void Cancel();
    CompletionDecision Complete(std::uint64_t generation);

private:
    std::uint64_t latestGeneration_ = 0;
    std::uint64_t activeGeneration_ = 0;
    std::uint64_t pendingGeneration_ = 0;
};

}  // namespace capture_request
