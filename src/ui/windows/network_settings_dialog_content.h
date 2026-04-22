#pragma once

#include <QDialog>

class QLineEdit;

namespace shatv::ui::windows {

class NetworkSettingsDialog final : public QDialog {
    Q_OBJECT

   public:
    explicit NetworkSettingsDialog(const QString &user_agent, const QString &epg_url, QWidget *parent = nullptr);

    QString UserAgent() const;
    QString EpgUrl() const;

   private:
    QLineEdit *user_agent_edit_ = nullptr;
    QLineEdit *epg_url_edit_ = nullptr;
};

}  // namespace shatv::ui::windows
