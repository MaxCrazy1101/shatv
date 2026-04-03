#include "ui/panels/playback_status_panel.h"

#include <QFormLayout>

#include "domain/playback_state.h"

namespace shatv::ui::panels {

PlaybackStatusPanel::PlaybackStatusPanel(QWidget *parent) : QWidget(parent) {
    channel_value_label_ = new QLabel(tr("None"), this);
    state_value_label_ = new QLabel(tr("Idle"), this);
    message_value_label_ = new QLabel(tr("Ready"), this);
    message_value_label_->setWordWrap(true);

    auto *layout = new QFormLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addRow(tr("Channel"), channel_value_label_);
    layout->addRow(tr("State"), state_value_label_);
    layout->addRow(tr("Message"), message_value_label_);
}

void PlaybackStatusPanel::ApplySnapshot(const domain::PlayerSnapshot &snapshot) {
    channel_value_label_->setText(snapshot.channel_name.isEmpty() ? tr("None") : snapshot.channel_name);
    state_value_label_->setText(domain::PlaybackStateName(snapshot.state));
    const QString message_text = snapshot.retry_count > 0
                                     ? tr("%1 (retry %2)").arg(snapshot.message).arg(snapshot.retry_count)
                                     : snapshot.message;
    message_value_label_->setText(message_text.isEmpty() ? tr("Ready") : message_text);
}

QString PlaybackStatusPanel::CurrentStateText() const {
    return state_value_label_->text();
}

}  // namespace shatv::ui::panels
