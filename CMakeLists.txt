cmake_minimum_required(VERSION 3.16)

project(ShaTV LANGUAGES CXX)

# 为 clangd 等工具导出编译数据库，便于后续开发。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(BUILD_TESTING "Build ShaTV tests" OFF)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SHATV_MPV_INCLUDE_DIR "" CACHE PATH "Path to libmpv headers for Windows builds")
set(SHATV_MPV_LIBRARY "" CACHE FILEPATH "Path to libmpv import library for Windows builds")
set(SHATV_MPV_DLL "" CACHE FILEPATH "Path to libmpv runtime DLL for Windows portable packaging")

if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Network)
    find_package(Qt6 QUIET COMPONENTS LinguistTools)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Network LinguistTools)
endif()
find_package(toml11 CONFIG REQUIRED)

include(FindShatvMpv)
shatv_configure_mpv_target(SHATV_MPV_TARGET)

add_subdirectory(src)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
