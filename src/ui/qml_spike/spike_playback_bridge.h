#pragma once

#include <optional>

#include <QObject>
#include <QPointer>
#include <QString>

#include "domain/channel.h"
#include "domain/player_snapshot.h"

namespace shatv::application {
class PlayerController;
}

namespace shatv::ui::qml_spike {

class MpvVideoItem;

class SpikePlaybackBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString sourceLabel READ sourceLabel NOTIFY sourceLabelChanged)
    Q_PROPERTY(QString playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(bool videoReady READ videoReady NOTIFY videoReadyChanged)

   public:
    explicit SpikePlaybackBridge(QObject *parent = nullptr);

    void SetController(application::PlayerController *controller);
    void SetVideoItem(MpvVideoItem *video_item);
    void SetStartupChannel(const std::optional<domain::Channel> &channel);

    QString statusMessage() const;
    QString sourceLabel() const;
    QString playbackState() const;
    bool videoReady() const;

    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void stop();

   signals:
    void statusMessageChanged();
    void sourceLabelChanged();
    void playbackStateChanged();
    void videoReadyChanged();

   private:
    void ApplySnapshot(const domain::PlayerSnapshot &snapshot);

    QPointer<application::PlayerController> controller_;
    QPointer<MpvVideoItem> video_item_;
    domain::PlayerSnapshot snapshot_;
    QString source_label_;
    bool video_ready_ = false;
};

}  // namespace shatv::ui::qml_spike
