# ShaTV TODO

## 当前优先级

- [x] 验证 Qt6 工程可用
- [x] 搭建最小应用骨架
- [x] 在骨架稳定后接入 `libmpv`

## 阶段 1：验证 Qt6

- [x] 在 `CMakeLists.txt` 和 `src/CMakeLists.txt` 中显式接入 `Qt6::Core`、`Qt6::Widgets`
- [x] 将 `src/main.cpp` 替换为最小 `QApplication + QMainWindow`
- [x] 运行 `cmake -S . -B build`
- [x] 运行 `cmake --build build`
- [x] 启动程序，确认 Qt6 窗口能够拉起

## 阶段 2：搭建最小应用骨架

- [x] 创建目录：`src/app`、`src/application`、`src/domain`、`src/player`、`src/ui/windows`、`src/ui/models`、`src/ui/panels`
- [x] 定义领域类型：`Channel`、`PlaybackState`、`PlayerSnapshot`
- [x] 定义 `PlayerBackend` 抽象接口
- [x] 提供 `FakePlayerBackend`，用于替代真实播放内核验证控制流
- [x] 实现 `PlayerController`
- [x] 实现 `ChannelListModel`
- [x] 实现最小 `MainWindow`
- [x] 提供占位视频区域，先不接入真实 `libmpv`
- [x] 在装配层连接 `MainWindow -> PlayerController -> FakePlayerBackend`
- [x] 使用硬编码演示频道验证切台与状态回流

## 阶段 3：接入真实播放后端

- [x] 引入 `MpvPlayerBackend`
- [x] 将占位视频区域替换为 `MpvRenderWidget + libmpv render API`
- [x] 验证播放状态从后端正确回流到 UI
- [x] 补充错误状态与自动重试最小闭环

## 暂缓项

- [ ] `EPG`
- [ ] 录制
- [ ] 时移
- [ ] `RTSP` / `UDP multicast`
- [ ] `QML` 前端
