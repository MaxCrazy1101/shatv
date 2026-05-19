#pragma once

#include <QString>
#include <vector>

#include "app/open_request.h"

namespace shatv::app {

class AppSettings final {
   public:
    explicit AppSettings(QString config_path = DefaultConfigPath());

    static QString DefaultConfigPath();

    const QString &ConfigPath() const;
    const QString &EpgUrl() const;
    const QString &UserAgent() const;
    int OsdAutoHideSeconds() const;
    int Volume() const;
    bool Muted() const;
    bool SpeechSubtitleEnabled() const;
    const std::vector<RecentOpenItem> &RecentItems() const;
    void SetEpgUrl(const QString &epg_url);
    void SetUserAgent(const QString &user_agent);
    void SetOsdAutoHideSeconds(int seconds);
    void SetVolume(int volume);
    void SetMuted(bool muted);
    void SetSpeechSubtitleEnabled(bool enabled);
    void RememberRecentItem(RecentOpenItem item);

    bool Load();
    bool Save() const;

   private:
    QString config_path_;
    QString epg_url_;
    QString user_agent_;
    int osd_auto_hide_seconds_ = 3;
    int volume_ = 50;
    bool muted_ = false;
    bool speech_subtitle_enabled_ = false;
    std::vector<RecentOpenItem> recent_items_;
};

}  // namespace shatv::app
