# ShaTV

ShaTV 是一个跨平台 IPTV 播放器项目，当前阶段采用 `Qt6 Widgets + libmpv render API` 构建桌面播放器壳，并保留 `FakePlayerBackend` 作为骨架 smoke 路径。

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
./build/src/shatv --open-media ~/下载/iptv.m3u
./build/src/shatv --open-media http://127.0.0.1:8080/live.m3u8
./build/src/shatv --open-url http://127.0.0.1:8080/index.m3u8
./build/src/shatv --open-url 'https://live.example.com/iptv.m3u?userid=123&token=456'
```

本地 `.m3u` 文件可通过菜单 `文件 -> 打开文件...` 导入为完整频道列表，并自动播放第一项。

远程 `.m3u` 链接也可通过菜单 `文件 -> 打开链接...` 导入，导入请求会复用已保存的 `User-Agent`。

桌面环境启动：

```bash
./build/src/shatv
```

桌面菜单入口：

- `文件 -> 打开文件...`：打开本地媒体文件，并替换当前左侧频道列表
- `文件 -> 打开链接...`：输入 `http://` 或 `https://` 链接，并立即播放
- `文件 -> 最近打开`：展示最近 `5` 条文件或链接记录，启动后可再次选择打开
- `设置 -> 网络设置...`：配置持久化 `User-Agent`

多语言支持：

- 当前内置 `zh_CN` 翻译资源
- 应用启动时会根据系统 locale 自动加载可用翻译

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

## Windows 构建

仓库提供 [`.github/workflows/windows-portable.yml`](.github/workflows/windows-portable.yml)，会在 `push` 和 `pull_request` 上使用 `windows-latest` 构建 `Release`，并上传 `shatv-windows-x64-portable.zip` artifact。

当前 CI 固定使用 shinchiro 的 `20260411` `mpv-dev` Windows 预编译包：

- 发布页：`https://github.com/shinchiro/mpv-winbuild-cmake/releases/tag/20260411`
- 二进制直链：`https://github.com/shinchiro/mpv-winbuild-cmake/releases/download/20260411/mpv-dev-x86_64-v3-20260411-git-3e3048a.7z`

Windows 便携包解压后至少包含：

- `shatv.exe`
- Qt runtime 与 `windeployqt` 收集到的插件
- `platforms/qwindows.dll`
- `qml/QtQuick/Controls/`
- `qml/QtQuick/Layouts/`
- `libmpv-2.dll`
- `NOTICE.txt`
- `THIRD_PARTY_SOURCES.md`
- `licenses/`

Windows CI 打包阶段不再依赖外网拉取 GNU 许可证正文，artifact 使用仓库内的 [packaging/licenses/LGPL-3.0.txt](/home/alex/code/shatv/packaging/licenses/LGPL-3.0.txt) 和 [packaging/licenses/LGPL-2.1.txt](/home/alex/code/shatv/packaging/licenses/LGPL-2.1.txt)。

本地 Windows 构建前提：

- Visual Studio 2022 / MSVC x64 工具链
- Qt `6.8.2` MSVC 2022 x64
- Ninja
- 上述 shinchiro `mpv-dev` 预编译包

shinchiro 当前 `mpv-dev` 包自带头文件和 `libmpv-2.dll`，但不直接提供 MSVC 可用的 `mpv.lib`。CI 会在 workflow 内根据 DLL 导出表生成 import library，本地如果也走 MSVC，需要先执行同样的步骤：

```powershell
$exports = & dumpbin /exports C:\deps\mpv\libmpv-2.dll |
  Select-String '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+\S+$' |
  ForEach-Object { $_.Line -replace '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+', '    ' }

@(
  'LIBRARY libmpv-2.dll'
  'EXPORTS'
) + $exports | Set-Content C:\deps\mpv\libmpv.def

lib /def:C:\deps\mpv\libmpv.def /machine:x64 /out:C:\deps\mpv\mpv.lib
```

生成 `mpv.lib` 后可以配置并编译：

```powershell
cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.8.2/msvc2022_64 `
  -DBUILD_TESTING=OFF `
  -DSHATV_MPV_INCLUDE_DIR=C:/deps/mpv/include `
  -DSHATV_MPV_LIBRARY=C:/deps/mpv/mpv.lib `
  -DSHATV_MPV_DLL=C:/deps/mpv/libmpv-2.dll
cmake --build build-windows --target shatv --config Release
windeployqt.exe --release --qmldir C:/path/to/shatv/src/ui/qml build-windows/src/shatv.exe
cmake -DPORTABLE_PACKAGE_DIR=C:/path/to/shatv/build-windows/src `
  -P packaging/windows/validate-portable-package.cmake
```

`--qmldir` 不能省略。当前主界面 `MainWindow.qml` 通过资源路径加载，并依赖 `QtQuick.Controls` / `QtQuick.Layouts`；如果只运行 `windeployqt.exe --release shatv.exe`，Windows 便携包里通常不会带上这些 QML 模块，启动时会报：

```text
module "QtQuick.Controls" is not installed
module "QtQuick.Layouts" is not installed
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
