#pragma once

#include <vector>

#include <QString>

namespace shatv::app {

struct RecentOpenItem {
    QString kind;
    QString target;
    QString label;
};

class AppSettings final {
   public:
    explicit AppSettings(QString config_path = DefaultConfigPath());

    static QString DefaultConfigPath();

    const QString &ConfigPath() const;
    const QString &UserAgent() const;
    const std::vector<RecentOpenItem> &RecentItems() const;
    void SetUserAgent(const QString &user_agent);
    void RememberRecentItem(RecentOpenItem item);

    bool Load();
    bool Save() const;

   private:
    QString config_path_;
    QString user_agent_;
    std::vector<RecentOpenItem> recent_items_;
};

}  // namespace shatv::app
