# ShaTV 核心设计

## 目标

ShaTV 是一个桌面 IPTV 播放器，当前阶段优先保证：

- 可演示
- 可维护
- 可扩展
- Linux 本地开发顺畅
- 为 Windows 构建与分发预留空间

当前项目已经不再是“纯 Qt Widgets 壳层”，而是演进为：

- `QML shell`
- `C++ bridge`
- `Widgets/OpenGL` 视频视口
- `libmpv` 播放后端

因此后续架构演进目标不是“把所有代码推倒重来迁移到 QML”，而是在保持 `application / player / domain` 边界稳定的前提下，继续完善这套混合架构。

## 技术决策

- 语言标准：`C++20`
- 壳层 UI：`Qt Quick / QML`
- 壳层承载：`QQuickWidget`
- 视频容器：`Qt Widgets`
- 视频渲染控件：`QOpenGLWidget`
- 播放后端：`libmpv render API`
- 平台策略：`Linux first`，保留 `Windows` 兼容空间

选择原因：

- QML 更适合当前的主界面壳层、频道列表、状态栏、工具栏和后续视觉演进。
- `QQuickWidget` 允许在不重写播放器渲染链的前提下，把主界面迁移到 QML。
- `PlaybackViewport + MpvRenderWidget` 保留了成熟的 OpenGL 视频承载方式，避免当前阶段重做 `libmpv` 与 Qt Quick Scene Graph 的集成。
- `libmpv` 继续作为成熟播放内核，不在当前阶段自研解码、同步和渲染链路。

## 架构边界

- `ui`
  - 负责界面展示和用户交互
  - 当前采用 `QML shell + QWidget 视频视口` 的混合模式
  - 不直接调用底层 `mpv_handle`
- `application`
  - 负责播放流程协调、自动重试、配置下发和 UI 协调
- `player`
  - 负责 `libmpv` 集成、事件转换、视频渲染和播放控制
- `domain`
  - 定义稳定数据结构，例如 `Channel`、`PlayerSnapshot`、`PlaybackState`
- `app`
  - 负责对象装配和启动入口

核心原则：

- UI 不直接操作 `mpv_handle`
- 播放器原始事件先转换为内部 snapshot，再交给 UI 使用
- QML 不直接感知后端实现，统一通过桥接对象与模型交互
- 视频区继续由专门的 Widgets/OpenGL 组件承载，不把 Scene Graph 集成复杂度扩散到当前阶段

## 当前主界面结构

当前主界面不是传统 `QMainWindow + Splitter + 全 Widgets`，而是：

```text
QMainWindow
└── content_host_ : QWidget
    ├── qml_view_ : QQuickWidget
    │   └── MainWindow.qml
    │       ├── 顶部工具栏
    │       ├── 左侧频道区
    │       │   ├── 搜索框
    │       │   ├── 分组筛选
    │       │   └── ChannelListView
    │       └── 右侧内容区
    │           ├── videoHost : QQuickItem
    │           ├── 底部状态/工具区域
    │           └── 全屏状态切换入口
    └── playback_viewport_ : QWidget
        ├── MpvRenderWidget
        └── PlaybackOsdOverlay
```

这里的关键点是：

- `QQuickWidget` 负责 QML 壳层
- `videoHost` 只是 QML 中的几何占位区域
- `MainWindow` 负责把 `videoHost` 的几何同步给 `playback_viewport_`
- 真正的视频显示和 OSD 交互仍在 `playback_viewport_` 内完成

## 核心对象关系

### `MainWindow`

职责：

- 承载 `QQuickWidget`
- 创建并维护 `MainWindowBridge`
- 管理 `PlaybackViewport`
- 接收 `PlayerController` 的快照回流
- 把模型、状态、recent items、fullscreen 状态同步给 QML

`MainWindow` 不直接操作 `mpv_handle`，也不直接写播放后端逻辑。

### `MainWindowBridge`

职责：

- 作为 QML 与 C++ 主窗口之间的桥接层
- 向 QML 暴露：
  - 频道模型
  - 分组列表
  - 搜索文本
  - recent items
  - fullscreen 状态
  - status message
- 把 QML 操作转成 C++ signal，例如：
  - 激活频道
  - 打开文件
  - 打开链接
  - 打开 recent item
  - 切换全屏

桥接层只做状态与事件转接，不承载业务流程。

### `PlaybackViewport`

职责：

- 承载 `MpvRenderWidget`
- 叠加 `PlaybackOsdOverlay`
- 管理全屏下的 OSD 自动隐藏与唤起
- 继续作为视频显示的真实 QWidget 容器

它是 QML 外壳与视频渲染层之间的边界。

### `PlayerController`

职责：

- 协调播放器主链
- 统一处理播放快照
- 管理应用级自动重试策略
- 不依赖 QML、QQuickWidget、`MpvRenderWidget` 等具体视图实现

### `MpvPlayerBackend`

职责：

- 封装 `libmpv`
- 管理 `mpv_handle` 与 `mpv_render_context`
- 对外提供加载、暂停、停止、音量、静音等能力

## 依赖方向

当前真实依赖方向是：

```text
QML (MainWindow.qml)
    -> MainWindowBridge
        -> MainWindow
            -> PlayerController
                -> PlayerBackend
                    -> MpvPlayerBackend
```

视频显示链单独为：

```text
MainWindow
    -> PlaybackViewport
        -> MpvRenderWidget
            -> MpvPlayerBackend render path
```

禁止出现的反向依赖：

- `MainWindowBridge -> mpv_handle`
- `QML -> PlayerController`
- `MpvPlayerBackend -> MainWindow`
- `ChannelListModel -> PlayerController`

## 关键调用链

### 播放主链

```text
用户在 QML 频道列表点击频道
-> MainWindowBridge::activateChannelRow()
-> MainWindow 收到 ActivateChannelRequested(row)
-> MainWindow 从 ChannelFilterModel 取得选中项
-> MainWindow 调用 PlayerController::PlayChannel(channel)
-> PlayerController 调用 PlayerBackend::Load(channel)
-> MpvPlayerBackend 驱动 libmpv
-> 后端快照回流
-> MainWindow 更新：
   - PlaybackViewport
   - ChannelFilterModel / ChannelListModel 当前高亮
   - QML bridge 状态
```

### 搜索与分组筛选链

```text
用户在 QML 输入搜索或切换分组
-> MainWindowBridge::setSearchText() / setGroupFilter()
-> ChannelFilterModel 更新过滤条件
-> QML ListView 通过 model 自动刷新可见项
```

### 全屏与 OSD 链

```text
用户在 QML 或键盘触发全屏
-> MainWindow::ToggleFullscreen()
-> MainWindow 切换窗口态并同步 fullscreen 状态到 bridge
-> PlaybackViewport::SetFullscreenActive(true)
-> PlaybackOsdOverlay 管理显示/隐藏
```

### 配置链

```text
Application 启动
-> AppSettings::Load()
-> MainWindow::SetConfiguredUserAgent()
-> MainWindow::SetOsdAutoHideSeconds()
-> MainWindow::SetRecentItems()
-> MainWindowBridge 同步给 QML
```

## 当前目录结构

当前有效目录结构如下：

```text
src/
  app/
  application/
  domain/
  player/
  ui/
    models/
    panels/
    qml/
    widgets/
    windows/
tests/
  unit/
docs/
translations/
```

说明：

- `ui/qml/` 放 QML 壳层页面
- `ui/windows/` 放 QWidget 主窗口壳与桥接对象
- `ui/widgets/` 放视频区等 QWidget 组件
- `ui/panels/` 放 OSD 等可复用控件

## 当前阶段的技术立场

当前项目已经进入“**QML 外壳 + Widgets 视频层**”的混合架构。

因此当前阶段的正确方向是：

- 继续完善 QML 壳层
- 保持 `PlaybackViewport + MpvRenderWidget` 视频路径稳定
- 继续保持 `application / player / domain` 不受 UI 技术栈变化影响

当前阶段不建议做：

- 全量重写为纯 Qt Quick 视频渲染架构
- 让 QML 直接操作播放器后端
- 为了追求“纯 QML”打破当前清晰的层次边界

## 后续演进

- 短期：
  - 完善 QML 主界面交互
  - 继续补齐全屏、设置、状态展示和 About 等壳层能力
- 中期：
  - 如果 QML 壳层进一步稳定，可逐步把更多非视频控件从 Widgets 收口到 QML
- 长期：
  - 只有在明确需要更深的 Scene Graph 集成时，才评估是否将视频承载从 `QOpenGLWidget` 迁移到 Qt Quick 原生渲染项

当前架构目标不是“全面迁移到纯 QML”，而是维护这套对播放器项目更稳妥的混合方案。
