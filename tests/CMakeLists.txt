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
