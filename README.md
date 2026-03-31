# ShaTV

ShaTV 是一个最小化的 C++/CMake 项目骨架，用作后续新项目的起点。

当前模板保留了以下基础能力：

- `CMake` 顶层构建配置
- `clang-format` / `clang-tidy` 工程化配置
- 最小可执行程序入口
- 最小 `ctest` smoke test
- `toolchain.cmake` 编译工具链配置

## 目录结构

```text
.
├── .clang-format
├── .clang-tidy
├── .gitignore
├── CMakeLists.txt
├── README.md
├── src
│   ├── CMakeLists.txt
│   └── main.cpp
├── tests
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

如需显式使用仓库内工具链文件：

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake
cmake --build build
ctest --test-dir build --output-on-failure
```

## 设计文档

- [核心设计](docs/architecture.md)

## 作为模板新建项目

在上级目录解压模板压缩包后，建议按以下流程开始新项目：

```bash
tar -xzf ShaTV-cpp-template.tar.gz
mv shatv your-project-name
cd your-project-name
git init --initial-branch=main
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

如果你修改了项目名称，记得同步调整顶层 `CMakeLists.txt` 中的 `project(...)` 名称。
