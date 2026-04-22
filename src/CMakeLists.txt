configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/app/build_info.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/app/build_info.h
    @ONLY
)

add_library(shatv_app
    app/application.cpp
    app/app_settings.cpp
    app/epg_service.cpp
    app/launch_options.cpp
    app/m3u_playlist_parser.cpp
    app/xmltv_epg_payload.cpp
    app/xmltv_epg_parser.cpp
    application/player_controller.cpp
    player/fake_player_backend.cpp
    player/mpv_event_adapter.cpp
    player/mpv_player_backend.cpp
    player/player_backend.cpp
    ui/video/mpv_video_item.cpp
    ui/models/channel_filter_model.cpp
    ui/models/channel_list_model.cpp
    ui/shell/app_shell_bridge.cpp
)

target_include_directories(shatv_app
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(shatv_app
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        shatv_mpv
        toml11::toml11
        ZLIB::ZLIB
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
        ui/qml/controls/ThemedToolButton.qml
        ui/qml/controls/ThemedTextField.qml
        ui/qml/controls/ThemedItemDelegate.qml
        ui/qml/controls/ThemedComboBox.qml
        ui/qml/controls/ThemedSlider.qml
        ui/qml/controls/WindowControlButton.qml
        ui/qml/dialogs/AboutDialog.qml
        ui/qml/dialogs/NetworkSettingsDialog.qml
        ui/qml/dialogs/OpenUrlDialog.qml
)
