#pragma once

#include <cstdint>

#include <QMetaType>
#include <QString>
#include <QUrl>

#include "domain/channel.h"

namespace shatv::domain {

enum class SourceKind : uint8_t {
    kLocalFile = 0,
    kDirectRemoteMedia,
    kRemotePlaylistFetch,
    kPlaylistChannelLive,
};

enum class SourceOrigin : uint8_t {
    kManualOpenFile = 0,
    kManualOpenUrl,
    kStartupArgument,
    kRecentItem,
    kLocalPlaylist,
    kRemotePlaylist,
};

enum class RetryBackoff : uint8_t {
    kNone = 0,
    kFixed,
    kIncreasing,
};

struct RetryPolicy {
    int max_attempts = 0;
    int initial_delay_ms = 0;
    int max_delay_ms = 0;
    RetryBackoff backoff = RetryBackoff::kNone;
};

struct MediaSourceDescriptor {
    QString id;
    QString name;
    QUrl url;
    SourceKind source_kind = SourceKind::kLocalFile;
    SourceOrigin origin = SourceOrigin::kManualOpenFile;
    QString user_agent;
    RetryPolicy retry_policy;
};

struct ResolvedChannel {
    Channel channel;
    MediaSourceDescriptor source;
};

RetryPolicy RetryPolicyForSourceKind(SourceKind source_kind);

}  // namespace shatv::domain

Q_DECLARE_METATYPE(shatv::domain::MediaSourceDescriptor)
Q_DECLARE_METATYPE(shatv::domain::ResolvedChannel)
