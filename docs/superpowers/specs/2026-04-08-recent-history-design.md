# ShaTV 最近打开记录设计

**日期**: 2026-04-08

## 目标

为 ShaTV 增加“最近打开记录”能力，支持记住最近打开的本地文件和远程链接，并在下次启动后通过菜单再次选择打开。

本次目标明确限定为：

- 最多保留 `5` 条最近记录
- 只提供“最近打开”菜单，不自动恢复播放
- 不新增单独历史数据库或额外配置文件

## 当前上下文

当前应用已经具备以下打开入口：

- 菜单 `文件 -> 打开文件...`
- 菜单 `文件 -> 打开链接...`
- 命令行 `--open-media`
- 命令行 `--open-url`

现有配置由 `src/app/app_settings.h` / `src/app/app_settings.cpp` 负责，使用单个 `config.toml` 保存 `network.user_agent`。

当前 `MainWindow` 已在 `文件` 菜单中挂载打开动作，应用层 `Application` 统一处理实际打开逻辑。

## 范围

### 本次范围内

- 在现有 `config.toml` 中保存最近打开记录
- 同时支持记录本地文件和远程链接
- 启动时在 `文件` 菜单下展示 `Open Recent` 子菜单
- 点击最近记录后复用现有打开逻辑
- 最近记录按“最新在前”排序
- 相同目标去重
- 最多保留 `5` 条

### 本次明确不做

- 启动时自动恢复上次播放
- 记住播放进度、暂停状态、音量或窗口布局
- 清空历史按钮
- 独立 `history.toml`
- 搜索、分组、置顶、固定收藏

## 用户体验

### 打开后如何记录

- 用户通过 `打开文件...` 成功导入本地媒体或本地播放列表后，写入一条最近记录
- 用户通过 `打开链接...` 通过现有 URL 校验后，写入一条最近记录
- 命令行 `--open-media` / `--open-url` 若触发实际打开，也写入最近记录
- `--smoke-test` / `--mpv-smoke` 不写入最近记录

### 启动后如何展示

- `文件` 菜单下新增 `Open Recent` 子菜单
- 若没有任何最近记录，则该子菜单显示为禁用
- 若有最近记录，则按最新到最旧展示，最多 `5` 条
- 点击条目后立即按原始类型重新打开：
  - 文件走文件打开逻辑
  - 链接走链接打开逻辑

### 不自动恢复

即使最近记录存在，应用启动后仍保持当前行为：

- 不自动播放
- 不自动联网
- 不自动替换当前列表

## 数据模型

新增一个最小记录结构：

- `kind`
  - `file`
  - `url`
- `target`
  - 实际打开目标
- `label`
  - 菜单展示名称

说明：

- `target` 用于重新打开和去重
- `label` 用于菜单展示，避免启动时再次推导名称
- 去重键为 `(kind, target)`

## 存储格式

继续复用现有 `config.toml`，在其中新增历史节点：

```toml
[network]
user_agent = "ShaTV Desktop/1.0"

[[history.recent]]
kind = "url"
target = "https://example.com/live/index.m3u8"
label = "index.m3u8"

[[history.recent]]
kind = "file"
target = "/home/alex/media/news.m3u"
label = "news.m3u"
```

## 架构与职责

### `src/app/app_settings.h` / `src/app/app_settings.cpp`

职责扩展为：

- 继续管理 `network.user_agent`
- 负责加载和保存最近记录
- 负责最近记录去重、截断到 `5` 条

### `src/app/application.h` / `src/app/application.cpp`

职责扩展为：

- 在合适的打开路径上调用最近记录写入
- 将最近记录传给 `MainWindow`
- 响应 `MainWindow` 的“最近记录被点击”信号
- 将点击动作转发回现有 `OpenFile()` / `OpenUrl()`

### `src/ui/windows/main_window.h` / `src/ui/windows/main_window.cpp`

职责扩展为：

- 渲染 `Open Recent` 子菜单
- 根据传入的最近记录重建菜单项
- 点击菜单项时发出信号

`MainWindow` 不直接做持久化，也不直接决定去重或上限。

## 写入规则

- 每次记录新条目时，把它插到最前面
- 若 `(kind, target)` 已存在，则先删除旧项，再插到最前
- 插入后若总数超过 `5`，截断尾部

## 失败处理

最近记录属于辅助能力，不应阻断媒体打开主流程。

因此本次规则为：

- 打开流程成功进入现有应用路径后，尝试保存最近记录
- 若保存失败：
  - 当前媒体打开流程继续
  - 在 `stderr` 打印错误
  - 状态栏短暂提示一条保存失败消息
- 不弹 `QMessageBox`

## 测试策略

考虑到当前项目要求最小测试面，本次只扩展现有 `tests/unit/launch_options_test.cpp`：

- 验证最近记录可保存并重新加载
- 验证相同目标会被去重并移动到队首
- 验证最多只保留 `5` 条

UI 菜单本次不额外增加新的自动化测试，改用手工验证：

1. 打开一个 URL
2. 关闭并重启应用
3. 在 `文件 -> Open Recent` 中再次打开该 URL

## 成功标准

实现完成后，应满足：

- 打开文件或链接后，最近记录写入 `config.toml`
- 下次启动可在 `Open Recent` 中看到最近 `5` 条
- 点击后能按原始类型重新打开
- 启动时仍不自动恢复播放
