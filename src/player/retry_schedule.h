#pragma once

#include "domain/media_source.h"

namespace shatv::player {

struct RetryDecision {
    bool should_retry = false;
    int next_attempt = 0;
    int delay_ms = 0;
};

RetryDecision RetryDecisionForFailure(const domain::RetryPolicy &policy, int completed_attempt);

}  // namespace shatv::player
