cmake_minimum_required(VERSION 3.16)

project(ShaTV VERSION 0.1.0 LANGUAGES CXX)

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

set(SHATV_MPV_INCLUDE_DIR "" CACHE PATH "Path to libmpv headers for Windows builds")
set(SHATV_MPV_LIBRARY "" CACHE FILEPATH "Path to libmpv import library for Windows builds")
set(SHATV_MPV_DLL "" CACHE FILEPATH "Path to libmpv runtime DLL for Windows portable packaging")

if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Network)
    find_package(Qt6 QUIET COMPONENTS LinguistTools)
    # Windows 上几乎不存在 toml11 系统包，默认启用 FetchContent。
    option(SHATV_FETCH_TOML11 "Fetch toml11 during configure" ON)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Network LinguistTools)
    option(SHATV_FETCH_TOML11 "Fetch toml11 during configure" OFF)
endif()

find_package(toml11 CONFIG QUIET)
if(NOT toml11_FOUND)
    if(NOT SHATV_FETCH_TOML11)
        message(FATAL_ERROR
            "toml11 not found. Install it (e.g. 'sudo pacman -S toml11') "
            "or re-run with -DSHATV_FETCH_TOML11=ON.")
    endif()

    include(FetchContent)
    FetchContent_Declare(
        toml11
        GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
        GIT_TAG v4.4.0
    )
    FetchContent_MakeAvailable(toml11)
endif()

include(FindShatvMpv)
shatv_configure_mpv_target(SHATV_MPV_TARGET)

add_subdirectory(src)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
