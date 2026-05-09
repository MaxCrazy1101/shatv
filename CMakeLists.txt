cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. "
        "Use an out-of-source build directory, for example: "
        "cmake -S . -B build")
endif()

project(ShaTV VERSION 0.1.0 LANGUAGES CXX)

# 为 clangd 等工具导出编译数据库，便于后续开发。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(BUILD_TESTING "Build ShaTV tests" OFF)
option(SHATV_ENABLE_ASR "Build optional sherpa-onnx ASR development probes" OFF)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SHATV_BUILD_ID "unknown")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE SHATV_BUILD_ID
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE SHATV_GIT_RESULT
    )
    if(NOT SHATV_GIT_RESULT EQUAL 0 OR SHATV_BUILD_ID STREQUAL "")
        set(SHATV_BUILD_ID "unknown")
    endif()
endif()

set(SHATV_FFMPEG_INCLUDE_DIR "" CACHE PATH "Path to FFmpeg headers for Windows builds")
set(SHATV_FFMPEG_LIBRARY_DIR "" CACHE PATH "Path to FFmpeg import libraries for Windows builds")
set(SHATV_SHERPA_ONNX_ROOT "" CACHE PATH "Path to an extracted sherpa-onnx SDK for ASR-enabled builds")
set(SHATV_ONNXRUNTIME_ROOT "" CACHE PATH "Optional path to an ONNX Runtime SDK when it is not bundled with sherpa-onnx")

if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Core Network Qml Quick QuickControls2 QuickDialogs2 Multimedia ShaderTools ShaderToolsPrivate)
    find_package(Qt6 QUIET COMPONENTS LinguistTools)
    # Windows 上几乎不存在 toml11 系统包，默认启用 FetchContent。
    option(SHATV_FETCH_TOML11 "Fetch toml11 during configure" ON)
    option(SHATV_FETCH_ZLIB "Fetch zlib during configure" ON)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core Network Qml Quick QuickControls2 QuickDialogs2 LinguistTools Multimedia ShaderTools ShaderToolsPrivate)
    option(SHATV_FETCH_TOML11 "Fetch toml11 during configure" OFF)
    option(SHATV_FETCH_ZLIB "Fetch zlib during configure" OFF)
endif()

include(FetchContent)

find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    if(NOT SHATV_FETCH_ZLIB)
        message(FATAL_ERROR
            "ZLIB not found. Install it (e.g. 'sudo pacman -S zlib') "
            "or re-run with -DSHATV_FETCH_ZLIB=ON.")
    endif()
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "Disable zlib example binaries" FORCE)
    FetchContent_Declare(
        zlib
        URL https://github.com/madler/zlib/archive/refs/tags/v1.3.1.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(zlib)

    if(TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    elseif(TARGET zlib)
        add_library(ZLIB::ZLIB ALIAS zlib)
    else()
        message(FATAL_ERROR "Fetched zlib, but no usable library target was exported.")
    endif()
endif()

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

find_package(toml11 CONFIG QUIET)
if(NOT toml11_FOUND)
    if(NOT SHATV_FETCH_TOML11)
        message(FATAL_ERROR
            "toml11 not found. Install it (e.g. 'sudo pacman -S toml11') "
            "or re-run with -DSHATV_FETCH_TOML11=ON.")
    endif()

    FetchContent_Declare(
        toml11
        GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
        GIT_TAG v4.4.0
    )
    FetchContent_MakeAvailable(toml11)
endif()

include(FindFfmpeg)
shatv_configure_ffmpeg_target(SHATV_FFMPEG_TARGET)

if(SHATV_ENABLE_ASR)
    include(FindSherpaOnnx)
    shatv_configure_sherpa_onnx_target(SHATV_SHERPA_ONNX_TARGET)
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
