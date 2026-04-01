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
