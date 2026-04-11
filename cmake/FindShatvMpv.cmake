function(shatv_configure_mpv_target out_target)
    if(WIN32)
        if(NOT SHATV_MPV_INCLUDE_DIR OR NOT SHATV_MPV_LIBRARY)
            message(FATAL_ERROR "Windows build requires SHATV_MPV_INCLUDE_DIR and SHATV_MPV_LIBRARY")
        endif()

        add_library(shatv_mpv SHARED IMPORTED GLOBAL)
        set_target_properties(shatv_mpv PROPERTIES
            IMPORTED_IMPLIB "${SHATV_MPV_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SHATV_MPV_INCLUDE_DIR}"
        )

        if(SHATV_MPV_DLL)
            set_target_properties(shatv_mpv PROPERTIES
                IMPORTED_LOCATION "${SHATV_MPV_DLL}"
            )
        endif()
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

        add_library(shatv_mpv INTERFACE)
        target_link_libraries(shatv_mpv INTERFACE PkgConfig::MPV)
    endif()

    set(${out_target} shatv_mpv PARENT_SCOPE)
endfunction()
