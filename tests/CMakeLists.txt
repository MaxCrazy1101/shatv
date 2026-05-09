add_test(
    NAME shatv_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen $<TARGET_FILE:shatv> --smoke-test
)

set_tests_properties(
    shatv_smoke
    PROPERTIES PASS_REGULAR_EXPRESSION "ShaTV FFmpeg video smoke ok state=idle"
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

add_executable(source_open_service_test
    unit/source_open_service_test.cpp
)

target_link_libraries(source_open_service_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME source_open_service_test
    COMMAND $<TARGET_FILE:source_open_service_test>
)

add_executable(retry_schedule_test
    unit/retry_schedule_test.cpp
)

target_link_libraries(retry_schedule_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME retry_schedule_test
    COMMAND $<TARGET_FILE:retry_schedule_test>
)

add_executable(ffmpeg_player_backend_retry_test
    unit/ffmpeg_player_backend_retry_test.cpp
)

target_link_libraries(ffmpeg_player_backend_retry_test
    PRIVATE
        shatv_app
        Qt6::Test
)

target_compile_definitions(ffmpeg_player_backend_retry_test
    PRIVATE
        SHATV_TESTDATA_DIR="${CMAKE_SOURCE_DIR}/testdata"
)

add_test(
    NAME ffmpeg_player_backend_retry_test
    COMMAND $<TARGET_FILE:ffmpeg_player_backend_retry_test>
)

add_executable(logging_test
    unit/logging_test.cpp
)

target_link_libraries(logging_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME logging_test
    COMMAND $<TARGET_FILE:logging_test>
)

add_executable(app_settings_recent_test
    unit/app_settings_recent_test.cpp
)

target_link_libraries(app_settings_recent_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME app_settings_recent_test
    COMMAND $<TARGET_FILE:app_settings_recent_test>
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

add_executable(epg_programme_presentation_test
    unit/epg_programme_presentation_test.cpp
)

target_link_libraries(epg_programme_presentation_test
    PRIVATE
        shatv_app
        Qt6::Test
)

add_test(
    NAME epg_programme_presentation_test
    COMMAND $<TARGET_FILE:epg_programme_presentation_test>
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

if(SHATV_ENABLE_ASR)
    set(SHATV_ASR_PROBE_MODEL_DIR "" CACHE PATH "Model directory for the optional ASR probe CTest")
    set(SHATV_ASR_PROBE_AUDIO_FILE "" CACHE FILEPATH "Speech WAV fixture for the optional ASR probe CTest")

    add_test(
        NAME shatv_asr_probe_missing_args
        COMMAND ${CMAKE_COMMAND}
            -DSHATV_ASR_PROBE=$<TARGET_FILE:shatv_asr_probe>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_asr_probe_missing_args.cmake
    )

    if(SHATV_ASR_PROBE_MODEL_DIR AND SHATV_ASR_PROBE_AUDIO_FILE)
        add_test(
            NAME shatv_asr_probe
            COMMAND $<TARGET_FILE:shatv_asr_probe>
                --model-dir "${SHATV_ASR_PROBE_MODEL_DIR}"
                --audio-file "${SHATV_ASR_PROBE_AUDIO_FILE}"
        )
        set_tests_properties(
            shatv_asr_probe
            PROPERTIES PASS_REGULAR_EXPRESSION "ShaTV ASR probe ok text="
        )
    else()
        message(STATUS
            "ASR probe CTest is not registered because SHATV_ASR_PROBE_MODEL_DIR "
            "or SHATV_ASR_PROBE_AUDIO_FILE is unset.")
    endif()
endif()
