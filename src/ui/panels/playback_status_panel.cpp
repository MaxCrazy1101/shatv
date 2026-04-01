#include "ui/panels/playback_status_panel.h"

#include <QFormLayout>

#include "domain/playback_state.h"

namespace shatv::ui::panels {

PlaybackStatusPanel::PlaybackStatusPanel(QWidget *parent) : QWidget(parent) {
    channel_value_label_ = new QLabel("None", this);
    state_value_label_ = new QLabel("Idle", this);
    message_value_label_ = new QLabel("Ready", this);
    message_value_label_->setWordWrap(true);

    auto *layout = new QFormLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addRow("Channel", channel_value_label_);
    layout->addRow("State", state_value_label_);
    layout->addRow("Message", message_value_label_);
}

void PlaybackStatusPanel::ApplySnapshot(const domain::PlayerSnapshot &snapshot) {
    channel_value_label_->setText(snapshot.channel_name.isEmpty() ? "None" : snapshot.channel_name);
    state_value_label_->setText(domain::PlaybackStateName(snapshot.state));
    message_value_label_->setText(snapshot.message.isEmpty() ? "Ready" : snapshot.message);
}

QString PlaybackStatusPanel::CurrentStateText() const {
    return state_value_label_->text();
}

}  // namespace shatv::ui::panels
