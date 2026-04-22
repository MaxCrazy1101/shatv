#include "ui/windows/network_settings_dialog_content.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace shatv::ui::windows {

namespace {

QString Tr(const char *source_text) {
    return QCoreApplication::translate("shatv::ui::windows::NetworkSettingsDialogContent", source_text);
}

}  // namespace

NetworkSettingsDialog::NetworkSettingsDialog(const QString &user_agent, const QString &epg_url, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(Tr("Network Settings"));
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(12);

    auto *description = new QLabel(
        Tr("Configure the HTTP User-Agent and an optional XMLTV EPG URL used for current/next programme lookup."),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *form = new QFormLayout();
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);

    user_agent_edit_ = new QLineEdit(this);
    user_agent_edit_->setText(user_agent);
    user_agent_edit_->setPlaceholderText(Tr("Leave empty to use the default"));
    form->addRow(Tr("User-Agent"), user_agent_edit_);

    epg_url_edit_ = new QLineEdit(this);
    epg_url_edit_->setText(epg_url);
    epg_url_edit_->setPlaceholderText(Tr("https://example.com/guide.xml.gz"));
    form->addRow(Tr("EPG URL"), epg_url_edit_);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString NetworkSettingsDialog::UserAgent() const {
    return user_agent_edit_ != nullptr ? user_agent_edit_->text().trimmed() : QString();
}

QString NetworkSettingsDialog::EpgUrl() const {
    return epg_url_edit_ != nullptr ? epg_url_edit_->text().trimmed() : QString();
}

}  // namespace shatv::ui::windows
