function(shatv_configure_ffmpeg_target out_target)
    if(WIN32)
        if(NOT SHATV_FFMPEG_INCLUDE_DIR OR NOT SHATV_FFMPEG_LIBRARY_DIR)
            message(FATAL_ERROR
                "Windows build requires SHATV_FFMPEG_INCLUDE_DIR and SHATV_FFMPEG_LIBRARY_DIR")
        endif()

        add_library(shatv_ffmpeg INTERFACE)
        target_include_directories(shatv_ffmpeg INTERFACE "${SHATV_FFMPEG_INCLUDE_DIR}")
        target_link_directories(shatv_ffmpeg INTERFACE "${SHATV_FFMPEG_LIBRARY_DIR}")
        target_link_libraries(shatv_ffmpeg INTERFACE avformat avcodec avutil swresample swscale)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
            libavformat
            libavcodec
            libavutil
            libswresample
            libswscale
        )

        add_library(shatv_ffmpeg INTERFACE)
        target_link_libraries(shatv_ffmpeg INTERFACE PkgConfig::FFMPEG)
    endif()

    set(${out_target} shatv_ffmpeg PARENT_SCOPE)
endfunction()
