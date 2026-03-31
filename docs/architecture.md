# ShaTV 核心设计

## 目标

ShaTV 是一个面向 Linux 的 IPTV 视频流播放器，首版作为面试项目交付，后续可持续演进为开源项目。第一阶段要求优先实现可演示、可维护、可扩展的最小可用版本，并保留 Windows 构建支持的可能性。

## 技术决策

- 语言标准：`C++20`
- UI：`Qt Widgets`
- 播放后端：`libmpv render API`
- 渲染控件：`QOpenGLWidget`
- 平台策略：`Linux first`，保留 `Windows` 兼容空间

选择原因：

- `Qt Widgets` 更适合首版桌面播放器的频道列表、控制栏、状态面板和设置交互。
- `libmpv` 适合作为成熟播放内核嵌入应用，首版无需自行实现完整的解码、同步和渲染管线。
- `QOpenGLWidget` 便于在 Qt 界面内承载视频渲染，并为后续叠加自定义 UI 留出空间。

## 架构边界

- `ui`：只负责界面展示和用户交互，不直接调用底层 mpv API。
- `application`：负责协调 UI、播放后端和播放列表逻辑。
- `player`：负责 libmpv 集成、播放控制、事件转换和视频渲染。
- `playlist`：负责 M3U/M3U8 导入、解析、筛选和频道组织。
- `domain`：定义稳定的数据结构，例如 `Channel`、`Playlist`、`PlaybackState`。
- `infra`：放置日志、路径等通用支撑代码，避免与业务逻辑耦合。

核心原则：

- UI 不直接操作 `mpv_handle`
- 播放器原始事件先转换为内部状态，再交给 UI 使用
- 目录按职责组织，当前阶段不单独创建 `include/`
- 对象装配放在 `app` 层，不让 `application` 直接依赖具体 Qt Widget 实现

## 主窗口布局

首版主窗口采用桌面播放器布局，优先保证切台、状态展示和可演示性：

```text
QMainWindow
├── MenuBar
├── CentralWidget
│   └── QSplitter(水平)
│       ├── 左侧频道区
│       │   ├── 搜索框
│       │   ├── 分组筛选
│       │   └── ChannelListView + ChannelListModel
│       └── 右侧播放区
│           └── QVBoxLayout
│               ├── MpvRenderWidget
│               ├── PlayerControlBar
│               └── PlaybackStatusPanel
└── StatusBar
```

布局原则：

- 左侧频道区保持固定宽度，负责切台、搜索和分组。
- `MpvRenderWidget` 占据右侧主体区域，只负责视频显示。
- `PlaybackStatusPanel` 展示当前频道、播放状态、错误和重试信息，作为首版的可观测性面板。
- `StatusBar` 只显示短消息，不承担核心状态展示。

## 核心对象关系

四个核心对象的职责与依赖方向如下：

- `MainWindow`
  - 组装主界面
  - 持有 `ChannelListModel`
  - 接收用户输入并转交给 `PlayerController`
  - 接收播放状态更新并刷新界面
- `PlayerController`
  - 负责播放流程协调和应用级状态
  - 依赖 `PlayerBackend` 抽象接口
  - 不依赖 `MpvRenderWidget` 等具体 UI 控件
- `MpvPlayerBackend`
  - 封装 `libmpv`
  - 管理 `mpv_handle` 和 `mpv_render_context`
  - 对外提供加载、暂停、停止、音量等播放能力
- `ChannelListModel`
  - 只服务频道列表展示
  - 被动接收频道集合和当前播放频道 ID
  - 不参与播放决策

对象装配关系：

```text
AppAssembly
├── 创建 MpvPlayerBackend
├── 创建 PlayerController(PlayerBackend*)
├── 创建 ChannelListModel
├── 创建 MainWindow(PlayerController*, ChannelListModel*)
└── 创建 MpvRenderWidget 并绑定到 MpvPlayerBackend
```

依赖方向：

```text
UI(MainWindow, ChannelListModel)
    -> Application(PlayerController)
        -> Player(MpvPlayerBackend)
```

禁止出现的反向依赖：

- `MainWindow -> mpv_handle`
- `MpvPlayerBackend -> MainWindow`
- `ChannelListModel -> PlayerController`
- `PlayerController -> MpvRenderWidget`

## 关键调用链

播放主链：

```text
用户点击频道
-> MainWindow 取得选中 Channel
-> MainWindow 调用 PlayerController::playChannel(channel)
-> PlayerController 调用 MpvPlayerBackend::load(channel.url)
-> MpvPlayerBackend 驱动 libmpv
-> libmpv 回调事件
-> MpvPlayerBackend 转换为内部 snapshot/event
-> PlayerController 归一化状态
-> MainWindow 更新 StatusPanel / ControlBar / ChannelListModel
```

播放控制链：

```text
用户点击暂停/静音/音量
-> MainWindow
-> PlayerController
-> MpvPlayerBackend
-> 后端状态回流
-> MainWindow 刷新控件显示
```

错误恢复链：

```text
libmpv 报错或流中断
-> MpvPlayerBackend 生成错误事件
-> PlayerController 判断是否自动重试
-> 如需重试则再次调用 load(current_url)
-> MainWindow 更新重试中/失败状态
```

说明：

- 自动重试策略属于 `PlayerController`，不属于 `MpvPlayerBackend`。
- `ChannelListModel` 只在播放状态确定后被动更新当前高亮频道。
- `MpvRenderWidget` 负责 `QOpenGLWidget` 生命周期与绘制入口，真正渲染由后端提供能力。

## 目录结构

```text
src/
  app/
  application/
  domain/
  infra/
  player/
  playlist/
  ui/
    windows/
    panels/
    models/
tests/
  unit/
  smoke/
resources/
cmake/
docs/
```

说明：

- `.h/.cpp` 按模块放在同一目录，优先保证开发效率和职责清晰。
- 只有在出现明确的公共 API 或库化需求后，再引入 `include/shatv/`。

## MVP 范围

首版聚焦以下能力：

- 导入本地或远程 `M3U/M3U8`
- 支持 `HTTP/HLS` 播放
- 频道列表展示、搜索、分组
- 播放控制：播放、暂停、切台、静音、音量、全屏
- 播放状态展示：加载、缓冲、失败、重试
- 基础设置持久化

首版暂不纳入：

- `EPG`
- 录制
- 时移
- 复杂插件体系
- 为库分发设计的公共头文件结构

## 后续演进

- 第二阶段可增加 `RTSP`、`UDP multicast`、收藏和历史记录。
- 如需提升产品表现力，可在后续评估 `QML` 前端，但保持 `player/application` 层不变。
- 当播放器后端或播放列表模块具备稳定接口后，再考虑迁移到 `include/shatv/` 并拆分独立库。
