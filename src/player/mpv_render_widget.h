#pragma once

#include <QWidget>

#include "domain/player_snapshot.h"

namespace shatv::player {

class MpvRenderWidget final : public QWidget {
    Q_OBJECT

   public:
    explicit MpvRenderWidget(QWidget *parent = nullptr);

    void ApplySnapshot(const domain::PlayerSnapshot &snapshot);

   protected:
    void paintEvent(QPaintEvent *event) override;

   private:
    QString title_ = "No Channel Selected";
    QString subtitle_ = "Stage 2 Video Placeholder";
};

}  // namespace shatv::player
