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
    ui/models/channel_list_model.cpp
    ui/panels/player_control_bar.cpp
    ui/panels/playback_status_panel.cpp
    ui/windows/main_window.cpp
)

target_include_directories(shatv_app
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(shatv_app
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::OpenGLWidgets
        PkgConfig::MPV
        toml11::toml11
)

add_executable(shatv
    app/main.cpp
)

target_link_libraries(shatv
    PRIVATE
        shatv_app
)
