configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/app/build_info.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/app/build_info.h
    @ONLY
)

add_library(shatv_app
    app/application.cpp
    app/app_settings.cpp
    app/launch_options.cpp
    app/m3u_playlist_parser.cpp
    application/player_controller.cpp
    player/fake_player_backend.cpp
    player/mpv_event_adapter.cpp
    player/mpv_player_backend.cpp
    player/mpv_render_widget.cpp
    player/player_backend.cpp
    ui/models/channel_filter_model.cpp
    ui/models/channel_list_model.cpp
    ui/panels/playback_osd_overlay.cpp
    ui/panels/player_control_bar.cpp
    ui/panels/playback_status_panel.cpp
    ui/widgets/playback_viewport.cpp
    ui/windows/about_dialog_content.cpp
    ui/windows/main_window_bridge.cpp
    ui/windows/main_window.cpp
)

target_include_directories(shatv_app
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(shatv_app
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::OpenGLWidgets
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickWidgets
        shatv_mpv
        toml11::toml11
)

add_executable(shatv
    app/main.cpp
)

target_link_libraries(shatv
    PRIVATE
        shatv_app
)

if(TARGET Qt6::lrelease)
    qt_add_translations(shatv_app
        IMMEDIATE_CALL
        SOURCE_TARGETS shatv_app
        TS_FILES
            ${CMAKE_SOURCE_DIR}/translations/shatv_zh_CN.ts
    )
endif()

set_source_files_properties(
    ui/qml/MainWindow.qml
    PROPERTIES QT_RESOURCE_ALIAS MainWindow.qml
)

qt_add_resources(shatv_app shatv_app_main_window_qml
    PREFIX /qt/qml
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/ui/qml
    FILES
        ui/qml/MainWindow.qml
        ui/qml/Theme.qml
        ui/qml/qmldir
)
