#include "ui/windows/main_window.h"

#include <QAction>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include "application/player_controller.h"
#include "domain/playback_state.h"
#include "player/mpv_render_widget.h"
#include "ui/models/channel_filter_model.h"
#include "ui/models/channel_list_model.h"
#include "ui/panels/playback_status_panel.h"
#include "ui/panels/player_control_bar.h"
#include "ui/windows/about_dialog_content.h"
#include "ui/widgets/playback_viewport.h"

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
    connect(playback_viewport_, &widgets::PlaybackViewport::PlayPauseRequested, this, &MainWindow::OnPlayPauseRequested);
    connect(playback_viewport_, &widgets::PlaybackViewport::StopRequested, controller_,
            &application::PlayerController::Stop);
    connect(playback_viewport_, &widgets::PlaybackViewport::MuteToggled, this, &MainWindow::OnMuteToggled);
    connect(playback_viewport_, &widgets::PlaybackViewport::VolumeChanged, controller_,
            &application::PlayerController::SetVolume);
    connect(playback_viewport_, &widgets::PlaybackViewport::ExitFullscreenRequested, this,
            &MainWindow::ExitFullscreen);
}

void MainWindow::SetChannels(std::vector<domain::Channel> channels) {
    channel_model_->SetChannels(std::move(channels));
    RebuildGroupFilter();
}

void MainWindow::StartInitialPlayback() {
    if (channel_filter_model_->rowCount() <= 0) {
        return;
    }

    const QModelIndex first_channel = channel_filter_model_->index(0, 0);
    channel_list_view_->setCurrentIndex(first_channel);
    OnChannelActivated(first_channel);
}

void MainWindow::StartSmokeScenario() {
    StartInitialPlayback();
}

player::MpvRenderWidget *MainWindow::RenderWidget() const {
    return playback_viewport_->RenderWidget();
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

void MainWindow::SetConfiguredUserAgent(const QString &user_agent) {
    configured_user_agent_ = user_agent;
}

void MainWindow::SetOsdAutoHideSeconds(int seconds) {
    playback_viewport_->SetOsdAutoHideSeconds(seconds);
}

void MainWindow::SetRecentItems(std::vector<app::RecentOpenItem> items) {
    recent_items_ = std::move(items);
    RebuildRecentMenu();
}

domain::PlayerSnapshot MainWindow::LastAppliedSnapshot() const {
    return last_snapshot_;
}

void MainWindow::OnChannelActivated(const QModelIndex &index) {
    // 列表视图绑定的是代理模型，播放前需要映射回源模型。
    const QModelIndex source_index = channel_filter_model_->mapToSource(index);
    const domain::Channel channel = channel_model_->ChannelAt(source_index);
    if (channel.id.isEmpty()) {
        return;
    }

    controller_->PlayChannel(channel);
}

void MainWindow::OnPlaybackSnapshotChanged(const shatv::domain::PlayerSnapshot &snapshot) {
    // UI 层只消费归一化后的快照，不直接理解底层播放器事件。
    last_snapshot_ = snapshot;
    playback_viewport_->ApplySnapshot(snapshot);
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

void MainWindow::OnGroupFilterChanged(int index) {
    if (index < 0) {
        return;
    }

    channel_filter_model_->SetGroupFilter(group_filter_->itemData(index).toString());
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

    auto *view_menu = menuBar()->addMenu(tr("&View"));
    toggle_fullscreen_action_ = view_menu->addAction(tr("Toggle Full Screen"));
    toggle_fullscreen_action_->setShortcut(QKeySequence(Qt::Key_F11));
    addAction(toggle_fullscreen_action_);
    connect(toggle_fullscreen_action_, &QAction::triggered, this, &MainWindow::ToggleFullscreen);

    auto *help_menu = menuBar()->addMenu(tr("&Help"));
    auto *about_action = help_menu->addAction(tr("&About ShaTV..."));
    connect(about_action, &QAction::triggered, this, &MainWindow::OnAboutRequested);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    left_panel_ = new QWidget(splitter);
    auto *left_layout = new QVBoxLayout(left_panel_);
    left_layout->setContentsMargins(12, 12, 12, 12);
    left_layout->setSpacing(8);

    search_input_ = new QLineEdit(left_panel_);
    search_input_->setPlaceholderText(tr("Search channels"));
    group_filter_ = new QComboBox(left_panel_);
    channel_filter_model_ = new ui::models::ChannelFilterModel(this);
    channel_filter_model_->setSourceModel(channel_model_);
    channel_list_view_ = new QListView(left_panel_);
    channel_list_view_->setModel(channel_filter_model_);
    channel_list_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(group_filter_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::OnGroupFilterChanged);
    connect(search_input_, &QLineEdit::textChanged, channel_filter_model_, &ui::models::ChannelFilterModel::SetSearchText);
    RebuildGroupFilter();

    left_layout->addWidget(search_input_);
    left_layout->addWidget(group_filter_);
    left_layout->addWidget(channel_list_view_, 1);

    auto *right_panel = new QWidget(splitter);
    auto *right_layout = new QVBoxLayout(right_panel);
    right_layout->setContentsMargins(12, 12, 12, 12);
    right_layout->setSpacing(12);

    playback_viewport_ = new widgets::PlaybackViewport(right_panel);
    control_bar_ = new panels::PlayerControlBar(right_panel);
    status_panel_ = new panels::PlaybackStatusPanel(right_panel);

    right_layout->addWidget(playback_viewport_, 1);
    right_layout->addWidget(control_bar_);
    right_layout->addWidget(status_panel_);

    splitter->addWidget(left_panel_);
    splitter->addWidget(right_panel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 960});

    setCentralWidget(splitter);
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::ApplyFullscreenUiState(bool active) {
    if (fullscreen_active_ == active) {
        return;
    }

    fullscreen_active_ = active;
    if (fullscreen_active_) {
        was_maximized_before_fullscreen_ = isMaximized();
        menuBar()->hide();
        statusBar()->hide();
        left_panel_->hide();
        control_bar_->hide();
        status_panel_->hide();
        showFullScreen();
        playback_viewport_->SetFullscreenActive(true);
        return;
    }

    playback_viewport_->SetFullscreenActive(false);
    if (was_maximized_before_fullscreen_) {
        showMaximized();
    } else {
        showNormal();
    }
    menuBar()->show();
    statusBar()->show();
    left_panel_->show();
    control_bar_->show();
    status_panel_->show();
}

void MainWindow::RebuildGroupFilter() {
    const QString current_group = channel_filter_model_->GroupFilter();
    const QStringList groups = channel_filter_model_->AvailableGroups();

    QSignalBlocker blocker(group_filter_);
    group_filter_->clear();
    group_filter_->addItem(tr("All groups"), QString());
    for (const QString &group : groups) {
        group_filter_->addItem(group, group);
    }

    const int next_index = group_filter_->findData(current_group);
    group_filter_->setCurrentIndex(next_index >= 0 ? next_index : 0);
    channel_filter_model_->SetGroupFilter(group_filter_->currentData().toString());
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

void MainWindow::OnAboutRequested() {
    AboutDialog dialog(this);
    dialog.exec();
}

}  // namespace shatv::ui::windows
