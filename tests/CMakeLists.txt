add_test(
    NAME shatv_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen $<TARGET_FILE:shatv> --smoke-test
)

set_tests_properties(
    shatv_smoke
    PROPERTIES PASS_REGULAR_EXPRESSION "ShaTV Stage2 smoke ok channels=1 current=smoke-test state=playing"
)

add_executable(launch_options_test
    unit/launch_options_test.cpp
)

target_link_libraries(launch_options_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME launch_options_test
    COMMAND $<TARGET_FILE:launch_options_test>
)

add_executable(channel_filter_model_test
    unit/channel_filter_model_test.cpp
)

target_link_libraries(channel_filter_model_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME channel_filter_model_test
    COMMAND $<TARGET_FILE:channel_filter_model_test>
)

add_executable(playback_viewport_test
    unit/playback_viewport_test.cpp
)

target_link_libraries(playback_viewport_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME playback_viewport_test
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen $<TARGET_FILE:playback_viewport_test>
)

add_executable(main_window_fullscreen_test
    unit/main_window_fullscreen_test.cpp
)

target_link_libraries(main_window_fullscreen_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME main_window_fullscreen_test
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen $<TARGET_FILE:main_window_fullscreen_test>
)

add_executable(windows_dependency_config_test
    unit/windows_dependency_config_test.cpp
)

target_compile_definitions(windows_dependency_config_test
    PRIVATE
        SHATV_TOPLEVEL_CMAKELISTS="${CMAKE_SOURCE_DIR}/CMakeLists.txt"
        SHATV_SRC_CMAKELISTS="${CMAKE_SOURCE_DIR}/src/CMakeLists.txt"
)

target_link_libraries(windows_dependency_config_test
    PRIVATE
        Qt6::Test
)

add_test(
    NAME windows_dependency_config_test
    COMMAND $<TARGET_FILE:windows_dependency_config_test>
)

add_executable(m3u_playlist_parser_test
    unit/m3u_playlist_parser_test.cpp
)

target_link_libraries(m3u_playlist_parser_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME m3u_playlist_parser_test
    COMMAND $<TARGET_FILE:m3u_playlist_parser_test>
)
