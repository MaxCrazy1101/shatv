#pragma once

#include <QString>

namespace shatv::app {

class AppSettings final {
   public:
    explicit AppSettings(QString config_path = DefaultConfigPath());

    static QString DefaultConfigPath();

    const QString &ConfigPath() const;
    const QString &UserAgent() const;
    void SetUserAgent(const QString &user_agent);

    bool Load();
    bool Save() const;

   private:
    QString config_path_;
    QString user_agent_;
};

}  // namespace shatv::app
