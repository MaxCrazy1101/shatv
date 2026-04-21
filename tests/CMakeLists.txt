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

add_executable(mpv_video_item_spike_test
    unit/mpv_video_item_spike_test.cpp
)

target_compile_definitions(mpv_video_item_spike_test
    PRIVATE
        SHATV_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_include_directories(mpv_video_item_spike_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(mpv_video_item_spike_test
    PRIVATE
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
        shatv_app
)

add_test(
    NAME mpv_video_item_spike_test
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen $<TARGET_FILE:mpv_video_item_spike_test>
)

add_executable(mpv_render_host_test
    unit/mpv_render_host_test.cpp
)

target_link_libraries(mpv_render_host_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME mpv_render_host_test
    COMMAND $<TARGET_FILE:mpv_render_host_test>
)
