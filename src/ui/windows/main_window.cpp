#include "ui/windows/main_window.h"

#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include "application/player_controller.h"
#include "domain/playback_state.h"
#include "player/mpv_render_widget.h"
#include "ui/models/channel_list_model.h"
#include "ui/panels/playback_status_panel.h"
#include "ui/panels/player_control_bar.h"

namespace shatv::ui::windows {

MainWindow::MainWindow(application::PlayerController *controller, ui::models::ChannelListModel *channel_model,
                       QWidget *parent)
    : QMainWindow(parent), controller_(controller), channel_model_(channel_model) {
    Q_ASSERT(controller_ != nullptr);
    Q_ASSERT(channel_model_ != nullptr);

    BuildUi();

    connect(channel_list_view_, &QListView::clicked, this, &MainWindow::OnChannelActivated);
    connect(controller_, &application::PlayerController::PlaybackSnapshotChanged, this,
            &MainWindow::OnPlaybackSnapshotChanged);
    connect(control_bar_, &panels::PlayerControlBar::PlayPauseRequested, this, &MainWindow::OnPlayPauseRequested);
    connect(control_bar_, &panels::PlayerControlBar::StopRequested, controller_,
            &application::PlayerController::Stop);
    connect(control_bar_, &panels::PlayerControlBar::MuteToggled, this, &MainWindow::OnMuteToggled);
    connect(control_bar_, &panels::PlayerControlBar::VolumeChanged, controller_,
            &application::PlayerController::SetVolume);
}

void MainWindow::SetChannels(std::vector<domain::Channel> channels) {
    channel_model_->SetChannels(std::move(channels));
}

void MainWindow::StartInitialPlayback() {
    if (channel_model_->rowCount() <= 0) {
        return;
    }

    const QModelIndex first_channel = channel_model_->index(0, 0);
    channel_list_view_->setCurrentIndex(first_channel);
    OnChannelActivated(first_channel);
}

void MainWindow::StartSmokeScenario() {
    StartInitialPlayback();
}

player::MpvRenderWidget *MainWindow::RenderWidget() const {
    return render_widget_;
}

int MainWindow::ChannelCount() const {
    return channel_model_->rowCount();
}

QString MainWindow::CurrentChannelIdForSmoke() const {
    return channel_model_->CurrentChannelId();
}

domain::PlayerSnapshot MainWindow::LastAppliedSnapshot() const {
    return last_snapshot_;
}

void MainWindow::OnChannelActivated(const QModelIndex &index) {
    const domain::Channel channel = channel_model_->ChannelAt(index);
    if (channel.id.isEmpty()) {
        return;
    }

    controller_->PlayChannel(channel);
}

void MainWindow::OnPlaybackSnapshotChanged(const shatv::domain::PlayerSnapshot &snapshot) {
    // UI 层只消费归一化后的快照，不直接理解底层播放器事件。
    last_snapshot_ = snapshot;
    render_widget_->ApplySnapshot(snapshot);
    control_bar_->ApplySnapshot(snapshot);
    status_panel_->ApplySnapshot(snapshot);

    if (!snapshot.channel_id.isEmpty()) {
        channel_model_->SetCurrentChannelId(snapshot.channel_id);
    }
    if (!snapshot.message.isEmpty()) {
        statusBar()->showMessage(snapshot.message, 3000);
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

void MainWindow::BuildUi() {
    setWindowTitle("ShaTV");
    resize(1280, 720);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    auto *left_panel = new QWidget(splitter);
    auto *left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(12, 12, 12, 12);
    left_layout->setSpacing(8);

    search_input_ = new QLineEdit(left_panel);
    search_input_->setPlaceholderText("Search channels");
    group_filter_ = new QComboBox(left_panel);
    group_filter_->addItem("All groups");
    channel_list_view_ = new QListView(left_panel);
    channel_list_view_->setModel(channel_model_);
    channel_list_view_->setSelectionMode(QAbstractItemView::SingleSelection);

    left_layout->addWidget(search_input_);
    left_layout->addWidget(group_filter_);
    left_layout->addWidget(channel_list_view_, 1);

    auto *right_panel = new QWidget(splitter);
    auto *right_layout = new QVBoxLayout(right_panel);
    right_layout->setContentsMargins(12, 12, 12, 12);
    right_layout->setSpacing(12);

    render_widget_ = new player::MpvRenderWidget(right_panel);
    control_bar_ = new panels::PlayerControlBar(right_panel);
    status_panel_ = new panels::PlaybackStatusPanel(right_panel);

    right_layout->addWidget(render_widget_, 1);
    right_layout->addWidget(control_bar_);
    right_layout->addWidget(status_panel_);

    splitter->addWidget(left_panel);
    splitter->addWidget(right_panel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 960});

    setCentralWidget(splitter);
    statusBar()->showMessage("Ready");
}

}  // namespace shatv::ui::windows
