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

add_executable(xmltv_epg_parser_test
    unit/xmltv_epg_parser_test.cpp
)

target_link_libraries(xmltv_epg_parser_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME xmltv_epg_parser_test
    COMMAND $<TARGET_FILE:xmltv_epg_parser_test>
)

add_executable(epg_service_test
    unit/epg_service_test.cpp
)

target_link_libraries(epg_service_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME epg_service_test
    COMMAND $<TARGET_FILE:epg_service_test>
)

add_executable(xmltv_epg_payload_test
    unit/xmltv_epg_payload_test.cpp
)

target_link_libraries(xmltv_epg_payload_test
    PRIVATE
        shatv_app
        Qt6::Test
        ZLIB::ZLIB
)

add_test(
    NAME xmltv_epg_payload_test
    COMMAND $<TARGET_FILE:xmltv_epg_payload_test>
)
