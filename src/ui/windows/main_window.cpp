#include "ui/windows/main_window.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QQuickWidget>
#include <QQmlContext>
#include <QWidget>

#include "application/player_controller.h"
#include "domain/playback_state.h"
#include "player/mpv_player_backend.h"
#include "ui/models/channel_filter_model.h"
#include "ui/models/channel_list_model.h"
#include "ui/qml_spike/mpv_video_item.h"
#include "ui/windows/about_dialog_content.h"
#include "ui/windows/main_window_bridge.h"

namespace shatv::ui::windows {

MainWindow::MainWindow(application::PlayerController *controller, ui::models::ChannelListModel *channel_model,
                       QWidget *parent)
    : QMainWindow(parent), controller_(controller), channel_model_(channel_model) {
    Q_ASSERT(controller_ != nullptr);
    Q_ASSERT(channel_model_ != nullptr);

    BuildUi();

    connect(controller_, &application::PlayerController::PlaybackSnapshotChanged, this,
            &MainWindow::OnPlaybackSnapshotChanged);
    status_message_timer_.setSingleShot(true);
    connect(&status_message_timer_, &QTimer::timeout, this, [this]() {
        status_message_.clear();
        bridge_->SetStatusMessage(QString());
    });

    connect(bridge_, &MainWindowBridge::ActivateChannelRequested, this, &MainWindow::OnChannelActivated);
    connect(bridge_, &MainWindowBridge::PlayPauseRequested, this, &MainWindow::OnPlayPauseRequested);
    connect(bridge_, &MainWindowBridge::StopRequested, controller_, &application::PlayerController::Stop);
    connect(bridge_, &MainWindowBridge::MuteRequested, controller_, &application::PlayerController::SetMuted);
    connect(bridge_, &MainWindowBridge::VolumeRequested, controller_, &application::PlayerController::SetVolume);
    connect(bridge_, &MainWindowBridge::OpenFileRequested, this, &MainWindow::OnOpenFileRequested);
    connect(bridge_, &MainWindowBridge::OpenUrlRequested, this, &MainWindow::OnOpenUrlRequested);
    connect(bridge_, &MainWindowBridge::NetworkSettingsRequested, this, &MainWindow::OnNetworkSettingsRequested);
    connect(bridge_, &MainWindowBridge::AboutRequested, this, &MainWindow::OnAboutRequested);
    connect(bridge_, &MainWindowBridge::RecentOpenRequested, this,
            [this](const QString &kind, const QString &target) { emit RecentOpenSelected(kind, target); });
    connect(bridge_, &MainWindowBridge::ToggleFullscreenRequested, this, &MainWindow::ToggleFullscreen);
    connect(bridge_, &MainWindowBridge::ExitFullscreenRequested, this, &MainWindow::ExitFullscreen);
}

MainWindow::~MainWindow() {
    status_message_timer_.stop();
    if (video_item_ != nullptr) {
        video_item_->SetBackend(nullptr);
    }
}

void MainWindow::SetChannels(std::vector<domain::Channel> channels) {
    channel_model_->SetChannels(std::move(channels));
    RebuildGroupFilter();
}

void MainWindow::StartInitialPlayback() {
    if (channel_filter_model_->rowCount() <= 0) {
        return;
    }

    OnChannelActivated(channel_filter_model_->index(0, 0));
}

void MainWindow::StartSmokeScenario() {
    StartInitialPlayback();
}

void MainWindow::AttachMpvBackend(player::MpvPlayerBackend *backend) {
    if (video_item_ == nullptr) {
        return;
    }

    video_item_->SetBackend(backend);
}

void MainWindow::SetConfiguredUserAgent(const QString &user_agent) {
    configured_user_agent_ = user_agent;
}

void MainWindow::SetOsdAutoHideSeconds(int seconds) {
    Q_UNUSED(seconds);
    // 第一刀迁移先去掉 QWidget OSD 视口，自动隐藏策略后续在纯 QML overlay 中恢复。
}

void MainWindow::SetRecentItems(std::vector<app::RecentOpenItem> items) {
    recent_items_ = std::move(items);
    bridge_->SetRecentItems(recent_items_);
}

void MainWindow::ShowStatusMessage(const QString &message, int timeout_ms) {
    status_message_ = message;
    bridge_->SetStatusMessage(status_message_);
    status_message_timer_.stop();

    if (timeout_ms <= 0 || message.isEmpty()) {
        return;
    }

    status_message_timer_.start(timeout_ms);
}

int MainWindow::ChannelCount() const {
    return channel_model_->rowCount();
}

bool MainWindow::IsFullscreenModeActive() const {
    return fullscreen_active_;
}

QString MainWindow::CurrentChannelIdForSmoke() const {
    return channel_model_->CurrentChannelId();
}

domain::PlayerSnapshot MainWindow::LastAppliedSnapshot() const {
    return last_snapshot_;
}

void MainWindow::OnChannelActivated(const QModelIndex &index) {
    const QModelIndex source_index = channel_filter_model_->mapToSource(index);
    const domain::Channel channel = channel_model_->ChannelAt(source_index);
    if (channel.id.isEmpty()) {
        return;
    }

    controller_->PlayChannel(channel);
}

void MainWindow::OnPlaybackSnapshotChanged(const shatv::domain::PlayerSnapshot &snapshot) {
    last_snapshot_ = snapshot;
    bridge_->SetPlaybackSnapshot(snapshot);

    if (!snapshot.channel_id.isEmpty()) {
        channel_model_->SetCurrentChannelId(snapshot.channel_id);
    }
    if (!snapshot.message.isEmpty()) {
        ShowStatusMessage(snapshot.message, 3000);
    }

    emit UiSnapshotApplied(snapshot);
}

void MainWindow::OnPlayPauseRequested() {
    if (last_snapshot_.state == domain::PlaybackState::kPlaying ||
        last_snapshot_.state == domain::PlaybackState::kLoading) {
        controller_->Pause();
        return;
    }
    controller_->Resume();
}

void MainWindow::OnMuteToggled(bool muted) {
    controller_->SetMuted(muted);
}

void MainWindow::OnOpenFileRequested() {
    const QString selected_file = QFileDialog::getOpenFileName(
        this, tr("Open File"), QString(),
        tr("Media Files (*.m3u *.m3u8 *.mp4 *.mkv *.ts *.mov *.webm *.mp3 *.flac);;All Files (*)"));
    if (selected_file.isEmpty()) {
        return;
    }

    emit OpenFileSelected(selected_file);
}

void MainWindow::OnOpenUrlRequested() {
    bool accepted = false;
    const QString url_text = QInputDialog::getText(this, tr("Open Link"), tr("URL:"), QLineEdit::Normal, "http://",
                                                   &accepted);
    if (!accepted || url_text.trimmed().isEmpty()) {
        return;
    }

    emit OpenUrlSelected(url_text.trimmed());
}

void MainWindow::OnNetworkSettingsRequested() {
    bool accepted = false;
    const QString user_agent = QInputDialog::getText(this, tr("Network Settings"), tr("User-Agent:"),
                                                     QLineEdit::Normal,
                                                     configured_user_agent_, &accepted);
    if (!accepted) {
        return;
    }

    emit UserAgentChanged(user_agent.trimmed());
}

void MainWindow::ToggleFullscreen() {
    ApplyFullscreenUiState(!fullscreen_active_);
}

void MainWindow::ExitFullscreen() {
    if (!fullscreen_active_) {
        return;
    }

    ApplyFullscreenUiState(false);
}

void MainWindow::OnAboutRequested() {
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_F11) {
        ToggleFullscreen();
        event->accept();
        return;
    }
    if (fullscreen_active_ && event->key() == Qt::Key_Escape) {
        ExitFullscreen();
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::BuildUi() {
    setWindowTitle(tr("ShaTV"));
    resize(1280, 720);
    setFocusPolicy(Qt::StrongFocus);

    channel_filter_model_ = new ui::models::ChannelFilterModel(this);
    channel_filter_model_->setSourceModel(channel_model_);

    bridge_ = new MainWindowBridge(channel_filter_model_, this);

    qml_view_ = new QQuickWidget(this);
    qml_view_->setObjectName(QStringLiteral("mainWindowQmlView"));
    qml_view_->setResizeMode(QQuickWidget::SizeRootObjectToView);
    qml_view_->rootContext()->setContextProperty(QStringLiteral("mainWindowBridge"), bridge_);
    ui::qml_spike::RegisterQmlVideoTypes();
    setCentralWidget(qml_view_);

    qml_view_->setSource(QUrl(QStringLiteral("qrc:/qt/qml/MainWindow.qml")));
    qml_root_object_ = qml_view_->rootObject();
    Q_ASSERT(qml_root_object_ != nullptr);

    video_item_ =
        qobject_cast<ui::qml_spike::MpvVideoItem *>(qml_root_object_->findChild<QObject *>(QStringLiteral("playerVideoItem")));
    Q_ASSERT(video_item_ != nullptr);

    bridge_->SetAvailableGroups(channel_filter_model_->AvailableGroups());
    bridge_->SetCurrentGroupFilter(channel_filter_model_->GroupFilter());
    bridge_->SetSearchTextValue(channel_filter_model_->SearchText());
    bridge_->SetRecentItems(recent_items_);
    bridge_->SetFullscreenActive(fullscreen_active_);
    bridge_->SetStatusMessage(status_message_);
    bridge_->SetPlaybackSnapshot(last_snapshot_);
}

void MainWindow::ApplyFullscreenUiState(bool active) {
    if (fullscreen_active_ == active) {
        return;
    }

    fullscreen_active_ = active;
    bridge_->SetFullscreenActive(fullscreen_active_);

    if (fullscreen_active_) {
        was_maximized_before_fullscreen_ = isMaximized();
        showFullScreen();
    } else {
        if (was_maximized_before_fullscreen_) {
            showMaximized();
        } else {
            showNormal();
        }
    }
}

void MainWindow::RebuildGroupFilter() {
    const QStringList groups = channel_filter_model_->AvailableGroups();
    QString current_group = channel_filter_model_->GroupFilter();

    if (!current_group.isEmpty() && !groups.contains(current_group)) {
        current_group.clear();
        channel_filter_model_->SetGroupFilter(current_group);
    }

    bridge_->SetAvailableGroups(groups);
    bridge_->SetCurrentGroupFilter(current_group);
}

}  // namespace shatv::ui::windows
