# ShaTV M3U 导入设计

**日期**: 2026-04-02

## 目标

为 ShaTV 增加 IPTV `M3U` 播放列表导入能力，支持：

- 从本地 `.m3u` 文件导入频道列表
- 从远程 `.m3u` 链接下载并导入频道列表
- 用解析出的完整频道列表替换左侧列表
- 自动播放第一项
- 远程导入请求复用当前持久化 `User-Agent`

导入失败时必须弹出错误提示，并保持当前频道列表不变。

## 当前上下文

当前打开链路已经支持：

- 菜单 `文件 -> 打开文件...`
- 菜单 `文件 -> 打开链接...`
- 命令行 `--open-media`
- 命令行 `--open-url`

现有实现中，本地文件和链接都会被当成单个媒体目标处理，入口在：

- `src/ui/windows/main_window.cpp`
- `src/app/application.cpp`
- `src/app/launch_options.cpp`

当前没有播放列表解析能力，也没有远程文本下载逻辑。

## 范围

本次只实现最小可用的 IPTV `M3U` 导入，不做额外扩展。

### 本次范围内

- 解析标准 `#EXTM3U` / `#EXTINF` 风格的 IPTV `M3U`
- 识别本地 `.m3u` 文件
- 识别远程 `.m3u` 链接
- 在必要时通过文本内容识别“远程链接是否是频道列表”
- 将解析结果映射成 `std::vector<domain::Channel>`
- 替换左侧频道列表并自动播放第一项
- 下载远程 `M3U` 时附带已保存的 `User-Agent`
- 导入失败时弹框并保持当前列表不变

### 本次明确不做

- `EPG` 展示
- `tvg-logo` 图片展示
- 最近导入记录
- 播放列表编辑/保存
- `Xtream` / `JSON` / 其他源格式
- 对 `domain::Channel` 增加 logo、epg 等字段

## 输入判定规则

### 本地文件

- 扩展名为 `.m3u`
  - 按 IPTV 播放列表解析
- 扩展名为 `.m3u8`
  - 继续按单个媒体流处理
- 其他文件
  - 继续按当前单媒体逻辑处理

### 远程链接

- URL 明确以 `.m3u` 结尾
  - 下载文本并按频道列表解析
- URL 明确以 `.m3u8` 结尾
  - 默认按单个媒体流处理，不当作频道列表
- URL 无明显扩展名
  - 如果返回文本内容符合多项 `#EXTINF + URL` 结构，则按频道列表解析
  - 否则回退到当前单媒体逻辑

这样可以避免把普通 HLS `.m3u8` 单流误判为 IPTV 频道列表。

## M3U 解析规则

新增一个最小解析模块，输入为整段 UTF-8 文本，输出为 `std::vector<domain::Channel>`。

### 支持的语法

- 可选 `#EXTM3U` 头
- 多组 `#EXTINF` + 下一条 URL
- 空行
- 注释行

### 解析行为

- 忽略空行
- 忽略非关键注释行
- 每个频道项以一条 `#EXTINF` 和其后的第一条有效 URL 构成
- 没有 URL 的 `#EXTINF` 项丢弃
- 孤立 URL 行如果前面没有 `#EXTINF`，默认丢弃

### 字段映射

- `Channel.name`
  - 优先使用 `#EXTINF` 逗号后的显示名
  - 如果为空，则回退到 `tvg-name`
  - 如果仍为空，再回退到 URL 派生出的文件名或主机名
- `Channel.group`
  - 使用 `group-title`
  - 若不存在则留空
- `Channel.url`
  - 使用 `#EXTINF` 后一条有效 URL
- `Channel.id`
  - 运行时生成稳定字符串，例如 `m3u-<index>-<name>`

### 本次保留但不落地到 UI 的属性

- `tvg-logo`
- `x-tvg-url`
- 其他扩展属性

这些字段本次不进入 `domain::Channel`，避免扩大模型范围。

## 架构与文件边界

### 新增

- `src/app/m3u_playlist_parser.h`
- `src/app/m3u_playlist_parser.cpp`

职责：

- 只负责把 `M3U` 文本解析为频道列表
- 不依赖 UI
- 不直接发网络请求

### 修改

- `src/app/application.h`
- `src/app/application.cpp`

职责：

- 判断打开目标是单媒体还是播放列表
- 执行本地文件读取
- 执行远程 `M3U` 下载
- 下载成功后调用解析器
- 导入成功时替换当前频道列表并自动播放第一项
- 导入失败时弹错误框并保持当前列表不变

- `src/app/app_settings.h`
- `src/app/app_settings.cpp`

职责不变：

- 提供已保存的 `User-Agent`

- `src/CMakeLists.txt`
- 顶层 `CMakeLists.txt`

职责：

- 接入 `Qt6::Network`

### 尽量不修改

- `src/ui/windows/main_window.cpp`
- `src/ui/windows/main_window.h`

原因：

- 菜单入口已经存在
- 这次增强是“打开逻辑升级”，不是新增 UI 结构

## 数据流

### 本地 `.m3u`

1. 用户点击 `文件 -> 打开文件...`
2. `MainWindow` 发出已有打开文件信号
3. `Application::OpenFile()` 检测扩展名为 `.m3u`
4. 读取文件文本
5. 调用 `M3uPlaylistParser`
6. 若解析成功且频道数大于 0：
   - `main_window_->SetChannels(parsed_channels)`
   - `main_window_->StartInitialPlayback()`
7. 若失败：
   - 弹错误框
   - 当前列表保持不变

### 远程 `.m3u`

1. 用户点击 `文件 -> 打开链接...`
2. `MainWindow` 发出已有打开链接信号
3. `Application::OpenUrl()` 判断目标应走播放列表导入
4. 使用 Qt 网络层下载文本
5. 请求头附带当前持久化 `User-Agent`
6. 下载成功后调用 `M3uPlaylistParser`
7. 若解析成功且频道数大于 0：
   - `main_window_->SetChannels(parsed_channels)`
   - `main_window_->StartInitialPlayback()`
8. 若下载失败或解析失败：
   - 弹错误框
   - 当前列表保持不变

### 单媒体保持不变

如果打开目标不是 `M3U` 播放列表，则继续走当前单媒体打开逻辑：

- 本地媒体仍按 `BuildOpenMediaChannel(...)`
- 远程单媒体仍按 `BuildOpenUrlChannel(...)`

## 远程下载实现

远程 `M3U` 下载采用 Qt 自带网络层实现，新增 `Qt6::Network` 依赖。

原因：

- 这是应用层导入逻辑，不属于 `mpv` 解码职责
- 可以直接复用持久化 `User-Agent`
- 错误处理更适合做成“弹窗 + 保持当前列表不变”

### 请求行为

- 仅用于拉取远程 `M3U` 文本
- 请求头写入 `User-Agent`
- 下载完成后在应用层判断是否为可解析播放列表

## 错误处理

以下情况都需要弹窗，并保持当前频道列表不变：

- 本地文件无法读取
- 远程链接下载失败
- 返回内容不是可识别的 `M3U`
- 解析后没有任何有效频道
- 远程链接内容为空

错误提示应直接说明失败原因，例如：

- `Failed to open playlist file`
- `Failed to download playlist`
- `Playlist contains no playable channels`
- `Playlist format is not supported`

## 兼容性与行为约束

- 命令行 `--open-url / --open-media` 现有行为不应被破坏
- 普通 `.m3u8` HLS 单流不得误导入为频道列表
- 本地媒体打开逻辑保持现状
- 当前 `User-Agent` 配置对远程 `M3U` 下载和远程媒体播放保持一致

## 测试策略

当前测试面较小，本次继续在现有 `launch_options_test` 基础上补最小覆盖，必要时新增一个小型解析测试文件。

### 至少覆盖的测试

- 解析本地 `M3U` 文本为多频道
- 正确提取 `group-title`
- `name` 优先取 `#EXTINF` 逗号后的显示名
- 忽略空行、坏行、没有 URL 的无效项
- `.m3u8` 仍走单媒体路径
- 导入失败时不替换当前频道列表

### 手工验证

- 打开本地 `~/下载/iptv.m3u`
  - 左侧显示完整频道列表
  - 自动播放第一项
- 打开远程 `.m3u` 链接
  - 能导入并自动播放
- 故意提供错误链接
  - 弹错误框
  - 当前列表保持不变
- 设置自定义 `User-Agent` 后导入远程 `M3U`
  - 服务端能收到自定义请求头

## 实施顺序

1. 增加 `M3U` 文本解析模块
2. 为解析规则补最小失败测试
3. 在 `Application` 中接入本地 `.m3u` 导入
4. 在 `Application` 中接入远程 `.m3u` 下载与解析
5. 接入 `Qt6::Network` 与远程 `User-Agent`
6. 运行自动化验证与手工导入验证

## 成功标准

满足以下条件即视为本次设计目标完成：

- 本地 `.m3u` 可导入为左侧频道列表
- 远程 `.m3u` 链接可导入为左侧频道列表
- 自动播放第一项
- 导入失败不清空当前列表
- 远程导入复用已保存的 `User-Agent`
- 不破坏现有单媒体打开路径与命令行入口
