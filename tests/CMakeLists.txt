add_test(
    NAME shatv_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen $<TARGET_FILE:shatv> --smoke-test
)

set_tests_properties(
    shatv_smoke
    PROPERTIES PASS_REGULAR_EXPRESSION "ShaTV Stage2 smoke ok channels=3 current=demo-news state=playing"
)

add_executable(shatv_unit_test
    unit/app_skeleton_test.cpp
)

target_link_libraries(shatv_unit_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME shatv_unit_test
    COMMAND $<TARGET_FILE:shatv_unit_test>
)

add_executable(mpv_event_adapter_test
    unit/mpv_event_adapter_test.cpp
)

target_link_libraries(mpv_event_adapter_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME mpv_event_adapter_test
    COMMAND $<TARGET_FILE:mpv_event_adapter_test>
)

add_executable(mpv_player_backend_state_test
    unit/mpv_player_backend_state_test.cpp
)

target_link_libraries(mpv_player_backend_state_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME mpv_player_backend_state_test
    COMMAND $<TARGET_FILE:mpv_player_backend_state_test>
)

add_executable(player_controller_retry_test
    unit/player_controller_retry_test.cpp
)

target_link_libraries(player_controller_retry_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME player_controller_retry_test
    COMMAND $<TARGET_FILE:player_controller_retry_test>
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

if(DEFINED SHATV_SMOKE_MEDIA AND NOT "${SHATV_SMOKE_MEDIA}" STREQUAL "")
    add_test(
        NAME shatv_mpv_smoke
        COMMAND ${CMAKE_COMMAND} -E env
            QT_QPA_PLATFORM=offscreen
            SHATV_SMOKE_MEDIA=${SHATV_SMOKE_MEDIA}
            $<TARGET_FILE:shatv> --mpv-smoke
    )

    set_tests_properties(
        shatv_mpv_smoke
        PROPERTIES PASS_REGULAR_EXPRESSION "ShaTV Stage3 mpv smoke ok state=playing"
    )
endif()
