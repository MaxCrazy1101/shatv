#include "ui/windows/main_window.h"

#include <QAction>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenuBar>
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

void MainWindow::SetConfiguredUserAgent(const QString &user_agent) {
    configured_user_agent_ = user_agent;
}

void MainWindow::SetRecentItems(std::vector<app::RecentOpenItem> items) {
    recent_items_ = std::move(items);
    RebuildRecentMenu();
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

void MainWindow::BuildUi() {
    setWindowTitle(tr("ShaTV"));
    resize(1280, 720);

    auto *file_menu = menuBar()->addMenu(tr("&File"));
    auto *open_file_action = file_menu->addAction(tr("Open &File..."));
    auto *open_url_action = file_menu->addAction(tr("Open &Link..."));
    recent_menu_ = file_menu->addMenu(tr("Open Recent"));
    connect(open_file_action, &QAction::triggered, this, &MainWindow::OnOpenFileRequested);
    connect(open_url_action, &QAction::triggered, this, &MainWindow::OnOpenUrlRequested);
    RebuildRecentMenu();

    auto *settings_menu = menuBar()->addMenu(tr("&Settings"));
    auto *network_settings_action = settings_menu->addAction(tr("&Network Settings..."));
    connect(network_settings_action, &QAction::triggered, this, &MainWindow::OnNetworkSettingsRequested);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    auto *left_panel = new QWidget(splitter);
    auto *left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(12, 12, 12, 12);
    left_layout->setSpacing(8);

    search_input_ = new QLineEdit(left_panel);
    search_input_->setPlaceholderText(tr("Search channels"));
    group_filter_ = new QComboBox(left_panel);
    group_filter_->addItem(tr("All groups"));
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
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::RebuildRecentMenu() {
    if (recent_menu_ == nullptr) {
        return;
    }

    recent_menu_->clear();
    if (recent_items_.empty()) {
        recent_menu_->setEnabled(false);
        return;
    }

    recent_menu_->setEnabled(true);
    for (const auto &item : recent_items_) {
        QAction *action = recent_menu_->addAction(item.label);
        action->setToolTip(item.target);
        action->setStatusTip(item.target);
        connect(action, &QAction::triggered, this, [this, item]() { emit RecentOpenSelected(item.kind, item.target); });
    }
}

}  // namespace shatv::ui::windows
