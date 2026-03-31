cmake_minimum_required(VERSION 3.16)

project(ShaTV LANGUAGES CXX)

# 为 clangd 等工具导出编译数据库，便于后续开发。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_subdirectory(src)
add_subdirectory(tests)
