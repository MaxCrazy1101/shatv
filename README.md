# ShaTV

ShaTV 是一个面向 Linux 的 IPTV 播放器项目，当前阶段采用 `Qt6 Widgets + libmpv render API` 构建桌面播放器壳，并保留 `FakePlayerBackend` 作为骨架 smoke 路径。

当前已具备以下基础能力：

- `Qt6` 应用启动与窗口拉起
- 最小应用分层：`app / application / domain / player / ui`
- `MainWindow -> PlayerController -> PlayerBackend` 的垂直切片
- `QOpenGLWidget + libmpv render API` 的真实视频渲染链路
- `FakePlayerBackend` 与 `MpvPlayerBackend` 双后端验证路径
- `mpv` 事件归一化与最小自动重试
- `QtTest` 单元测试
- 菜单栏打开本地文件 / 输入链接
- 基于 `config.toml` 的持久化 `User-Agent` 配置
- `clang-format` / `clang-tidy` / `CMake` 工程化配置

## 目录结构

```text
.
├── .clang-format
├── .clang-tidy
├── .gitignore
├── CMakeLists.txt
├── TODO.md
├── README.md
├── docs
│   └── architecture.md
├── src
│   ├── app
│   ├── application
│   ├── domain
│   ├── player
│   ├── ui
│   └── CMakeLists.txt
├── tests
│   ├── unit
│   └── CMakeLists.txt
└── toolchain.cmake
```

## 构建与测试

常规构建：

```bash
cmake -S . -B build -DBUILD_TESTING=OFF
cmake --build build
```

依赖说明：

- 需要系统已安装 `libmpv`
- 需要系统已安装 `toml11`，例如 Arch Linux 可执行 `sudo pacman -S toml11`

如需启用测试：

```bash
cmake -S . -B build-tests -DBUILD_TESTING=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

说明：

- `build/` 默认只生成客户端目标
- `build-tests/` 用于单独启用测试，避免在同一个构建目录里切换 `BUILD_TESTING` 产生旧 target 残留

手动验证 smoke 模式：

```bash
./build/src/shatv --smoke-test
```

手动验证 `libmpv` smoke：

```bash
env QT_QPA_PLATFORM=offscreen SHATV_SMOKE_MEDIA=/absolute/path/to/local.mp4 ./build/src/shatv --mpv-smoke
```

普通模式下直接打开本地文件或 URL：

```bash
./build/src/shatv --open-media ./docs/file_example_MP4_1920_18MG.mp4
./build/src/shatv --open-media http://127.0.0.1:8080/live.m3u8
./build/src/shatv --open-url http://127.0.0.1:8080/index.m3u8
```

桌面环境启动：

```bash
./build/src/shatv
```

桌面菜单入口：

- `文件 -> 打开文件...`：打开本地媒体文件，并替换当前左侧频道列表
- `文件 -> 打开链接...`：输入 `http://` 或 `https://` 链接，并立即播放
- `设置 -> 网络设置...`：配置持久化 `User-Agent`

配置文件位置：

```text
~/.config/shatv/config.toml
```

最小配置示例：

```toml
[network]
user_agent = "ShaTV Custom UA/1.0"
```

该 `User-Agent` 会对所有远程 HTTP/HLS 播放统一生效，包括：

- `--open-url`
- 菜单“打开链接...”
- 远程频道列表项

如需显式使用仓库内工具链文件：

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake -DBUILD_TESTING=OFF
cmake --build build
```

如需在工具链配置下启用测试：

```bash
cmake -S . -B build-tests -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake -DBUILD_TESTING=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## 本地 HLS 测试

仓库不保存第三方测试视频文件。请将本地媒体文件放在仓库外，或放到已忽略的 `local-media/` 目录，再运行：

```bash
bash scripts/start_local_hls_test.sh /absolute/path/to/input.mp4
```

脚本会：

- 使用 `ffmpeg` 将本地文件循环转成 HLS
- 在 `http://127.0.0.1:8080/index.m3u8` 提供本地测试流
- 在退出时清理 `ffmpeg` 和 `python3 -m http.server` 进程

示例：

```bash
mkdir -p local-media
bash scripts/start_local_hls_test.sh ./local-media/sample.mp4
```

## 设计文档

- [核心设计](docs/architecture.md)
- [执行清单](TODO.md)
