# ShaTV

ShaTV 是一个面向 Linux 的 IPTV 播放器骨架项目，当前阶段采用 `Qt6 Widgets` 构建桌面应用壳，并使用 `FakePlayerBackend` 验证 `MainWindow -> PlayerController -> PlayerBackend` 的基础控制流。

当前已具备以下基础能力：

- `Qt6` 应用启动与窗口拉起
- 最小应用分层：`app / application / domain / player / ui`
- `FakePlayerBackend` 驱动的垂直切片 smoke 测试
- `QtTest` 单元测试
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
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

手动验证 smoke 模式：

```bash
./build/src/shatv --smoke-test
```

桌面环境启动：

```bash
./build/src/shatv
```

如需显式使用仓库内工具链文件：

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake
cmake --build build
ctest --test-dir build --output-on-failure
```

## 设计文档

- [核心设计](docs/architecture.md)
- [执行清单](TODO.md)
