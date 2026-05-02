#include "player/retry_schedule.h"

#include <algorithm>

namespace shatv::player {

RetryDecision RetryDecisionForFailure(const domain::RetryPolicy &policy, int completed_attempt) {
    if (completed_attempt <= 0 || policy.max_attempts <= 1 || completed_attempt >= policy.max_attempts) {
        return {};
    }

    const int base_delay_ms = std::max(0, policy.initial_delay_ms);
    int delay_ms = 0;
    switch (policy.backoff) {
        case domain::RetryBackoff::kNone:
            delay_ms = 0;
            break;
        case domain::RetryBackoff::kFixed:
            delay_ms = base_delay_ms;
            break;
        case domain::RetryBackoff::kIncreasing:
            delay_ms = base_delay_ms * completed_attempt;
            break;
    }

    if (policy.max_delay_ms > 0) {
        delay_ms = std::min(delay_ms, policy.max_delay_ms);
    }

    return RetryDecision{
        .should_retry = true,
        .next_attempt = completed_attempt + 1,
        .delay_ms = delay_ms,
    };
}

}  // namespace shatv::player
