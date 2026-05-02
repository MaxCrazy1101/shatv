# ShaTV 核心设计

## 目标

ShaTV 当前的主线目标很明确：

- 保持桌面 IPTV 播放器的主链稳定
- 用纯 QML 壳层承载交互和视觉演进
- 用清晰的层次边界保证后续功能继续可维护
- 保持 Linux 本地开发顺畅，并为 Windows 构建保留兼容路径

当前主线已经完成从旧的 Widgets 过渡方案收敛，真实架构是：

- `QGuiApplication`
- `QQmlApplicationEngine`
- `ApplicationWindow` QML 壳层
- `AppShellBridge` C++/QML bridge
- `QQuickRhiItem` 承载 FFmpeg 解码后的视频帧显示
- FFmpeg 作为唯一播放后端和媒体核心入口

不再是：

- `QMainWindow`
- `QQuickWidget`
- `QWidget` 视频 sibling
- `QOpenGLWidget` 视频视口壳层

如果看到 `docs/superpowers/` 下还有旧设计文档，那些是迁移过程的历史资料，不代表当前主线实现。

## 技术决策

- 语言标准：`C++20`
- 应用宿主：`QGuiApplication`
- QML 引擎：`QQmlApplicationEngine`
- UI 壳层：`Qt Quick / QML`
- 视频承载：`QQuickRhiItem` / `VideoPresenterItem`
- 默认播放后端：`FfmpegPlayerBackend`
- 平台策略：`Linux first`，持续保留 `Windows` 兼容能力

选择这套结构的原因：

- `ApplicationWindow` 足以承载当前主界面、标题栏、频道栏、状态区和弹窗交互
- `VideoPresenterItem` 能把 FFmpeg 解码后的 YUV 视频帧收进 Qt Quick 场景，而不再需要 QWidget 混合层
- `AppShellBridge` 统一了 QML 状态读取与用户动作回传，避免 QML 直接碰播放器后端
- `PlayerController`、`PlayerBackend`、`domain` 层仍然可以保持独立，不受具体 UI 技术细节污染

## 当前入口与启动链

当前启动入口位于 `src/app/main.cpp`：

```text
main()
-> QGuiApplication
-> ParseLaunchOptions()
-> Application(qt_app, options)
-> Application::Run()
```

`Application` 是当前的组合根（composition root），负责：

- 创建 `PlayerBackend`
- 创建 `PlayerController`
- 创建 `ChannelListModel` / `ChannelFilterModel`
- 创建 `AppShellBridge`
- 创建 `QQmlApplicationEngine`
- 注册 `VideoPresenterItem` QML type
- 将 `appShellBridge` 注入 QML context
- 加载 `qrc:/qt/qml/MainWindow.qml`

## 架构边界

### `app`

职责：

- 作为组合根装配全局对象
- 处理启动参数、配置、网络访问、EPG 加载
- 连接 bridge、controller、model、backend

关键对象：

- `Application`
- `AppSettings`
- `LaunchOptions`
- `EpgService`
- `m3u/xmltv` 解析与 payload 解码

### `application`

职责：

- 封装播放器业务编排逻辑
- 统一播放、暂停、停止、静音、音量和快照回流

关键对象：

- `PlayerController`

### `player`

职责：

- 封装播放器后端实现
- 默认通过 FFmpeg 完成 demux / decode / audio output / video presentation
- 对外暴露稳定的播放器接口

关键对象：

- `PlayerBackend`
- `FfmpegPlayerBackend`
- `VideoFrameSink`

### `domain`

职责：

- 定义跨层稳定数据结构

关键对象：

- `Channel`
- `MediaSourceDescriptor`
- `ResolvedChannel`
- `PlaybackState`
- `PlayerSnapshot`

### `ui/models`

职责：

- 暴露给 QML 的列表模型与过滤模型

关键对象：

- `ChannelListModel`
- `ChannelFilterModel`

### `ui/shell`

职责：

- 作为 QML 与 C++ 之间唯一桥接对象
- 提供 `Q_PROPERTY`
- 暴露 `Q_INVOKABLE`
- 将 QML 动作转换为 Qt signal

关键对象：

- `AppShellBridge`

### `ui/video`

职责：

- 承载 Qt Quick 场景内的视频渲染项
- 把 Qt Quick 视频显示项与后端视频帧提交协议接上

关键对象：

- `VideoPresenterItem`

### `ui/qml`

职责：

- 承载真正的应用窗口与界面组件
- 组织标题栏、侧栏、视频区、控制区、状态区和各类弹窗

关键对象：

- `MainWindow.qml`
- `Theme.qml`
- `controls/*.qml`
- `dialogs/*.qml`

## 当前真实对象关系

主交互链：

```text
MainWindow.qml
    -> bridge (alias of appShellBridge)
        -> AppShellBridge
            -> Application signal wiring
                -> PlayerController
                    -> PlayerBackend
                        -> FfmpegPlayerBackend
```

视频渲染链：

```text
MainWindow.qml
    -> VideoPresenterItem
        -> FfmpegPlayerBackend::SetVideoFrameSink(...)
            -> media::video::VideoFrame
                -> QRhi Y/U/V texture upload
```

EPG 链：

```text
Application
    -> QNetworkAccessManager / local file
        -> DecodeXmltvPayload()
            -> EpgService::LoadXmltv()
                -> LookupNowNext()
                    -> AppShellBridge::SetProgrammeTexts(...)
                        -> MainWindow.qml
```

## 关键调用链

### 播放主链

```text
用户点击频道
-> MainWindow.qml 调用 bridge.activateChannelRow(index)
-> AppShellBridge 发出 ActivateChannelRequested(sourceIndex)
-> Application 取出 ResolvedChannel
-> PlayerController::PlayResolvedChannel(resolved_channel)
-> PlayerBackend::Load(resolved_channel.source)
-> FfmpegPlayerBackend 驱动 FFmpeg media core
-> PlaybackSnapshotChanged 回流
-> AppShellBridge::SetPlaybackSnapshot(...)
-> QML 更新播放状态、频道名、音量、静音状态
```

### 搜索 / 分组筛选链

```text
用户输入搜索词或切换分组
-> MainWindow.qml 调用 bridge.setSearchText() / bridge.setGroupFilter()
-> AppShellBridge 更新 ChannelFilterModel
-> ListView 自动刷新
```

### 打开文件 / 打开链接链

```text
QML Dialog / FileDialog 提交
-> bridge.submitOpenFile() / bridge.submitOpenUrl()
-> AppShellBridge 发出 OpenFileRequested / OpenUrlRequested
-> Application::ResolveOpenRequest(...)
-> SourceOpenService 解析播放列表、下载远程 M3U、或返回单一媒体 OpenResolution
-> Application::OpenChannels(...)
```

### 网络设置链

```text
SettingsWindow 提交 userAgent / epgUrl
-> bridge.submitNetworkSettings(...)
-> Application::UpdateNetworkSettings(...)
-> AppSettings::Save()
-> 后续 SourceOpenService / MediaSourceDescriptor 使用最新 User-Agent
-> ReloadEpg()
```

## 当前目录结构

```text
src/
  app/
  application/
  domain/
  player/
  ui/
    models/
    qml/
      controls/
      dialogs/
      fonts/
    shell/
    video/
tests/
  unit/
docs/
translations/
cmake/
scripts/
```

说明：

- `ui/qml/` 是纯 QML UI 资源目录
- `ui/shell/` 只放 bridge
- `ui/video/` 只放 Qt Quick 视频渲染项
- 不再有 `ui/windows/`、`ui/widgets/`、`ui/panels/` 这类当前主线目录

## 当前阶段的技术立场

当前项目的正确描述是：

- 纯 QML 壳层
- C++ bridge
- `QQuickRhiItem` / `VideoPresenterItem` 视频承载
- FFmpeg 播放后端

因此当前阶段应继续保持：

- QML 不直接调用播放器后端
- 所有 QML 动作先进入 `AppShellBridge`
- `Application` 继续承担组合根和顶层流程协调职责
- `PlayerController` 不依赖 QML 细节
- `domain` 继续作为稳定跨层数据协议

当前阶段不应回退到：

- 再引入 `QQuickWidget` / `QMainWindow`
- 恢复 QWidget 视频 sibling 架构
- 让 QML 直接持有媒体后端内部句柄

## 后续演进

短期方向：

- 继续补齐 QML 壳层交互
- 完善弹窗、状态展示、recent items、网络配置和 EPG 展示
- 继续补 smoke / unit 覆盖

中期方向：

- 在不破坏层次边界的前提下继续扩展 QML 组件体系
- 继续把 UI 状态收敛到 `AppShellBridge`

长期方向：

- 仅在确有收益时才评估更深的 Qt Quick render path 优化
- 保持架构边界比“追求更炫的技术形态”更优先
