#include "domain/media_source.h"

namespace shatv::domain {

RetryPolicy RetryPolicyForSourceKind(SourceKind source_kind) {
    switch (source_kind) {
        case SourceKind::kLocalFile:
            return RetryPolicy{};
        case SourceKind::kRemotePlaylistFetch:
            return RetryPolicy{
                .max_attempts = 2,
                .initial_delay_ms = 300,
                .max_delay_ms = 300,
                .backoff = RetryBackoff::kFixed,
            };
        case SourceKind::kDirectRemoteMedia:
            return RetryPolicy{
                .max_attempts = 2,
                .initial_delay_ms = 300,
                .max_delay_ms = 1000,
                .backoff = RetryBackoff::kFixed,
            };
        case SourceKind::kPlaylistChannelLive:
            return RetryPolicy{
                .max_attempts = 5,
                .initial_delay_ms = 300,
                .max_delay_ms = 3000,
                .backoff = RetryBackoff::kIncreasing,
            };
    }
    return RetryPolicy{};
}

}  // namespace shatv::domain
