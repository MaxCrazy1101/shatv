#pragma once

#include <QLabel>
#include <QWidget>

#include "domain/player_snapshot.h"

namespace shatv::ui::panels {

class PlaybackStatusPanel final : public QWidget {
    Q_OBJECT

   public:
    explicit PlaybackStatusPanel(QWidget *parent = nullptr);

    void ApplySnapshot(const domain::PlayerSnapshot &snapshot);
    QString CurrentStateText() const;

   private:
    QLabel *channel_value_label_ = nullptr;
    QLabel *state_value_label_ = nullptr;
    QLabel *message_value_label_ = nullptr;
};

}  // namespace shatv::ui::panels
