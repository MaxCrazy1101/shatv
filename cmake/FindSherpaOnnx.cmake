function(shatv_configure_sherpa_onnx_target out_target)
    if(NOT SHATV_SHERPA_ONNX_ROOT)
        message(FATAL_ERROR
            "SHATV_ENABLE_ASR=ON requires SHATV_SHERPA_ONNX_ROOT to point to an "
            "extracted sherpa-onnx SDK. ShaTV does not FetchContent or runtime-download "
            "sherpa-onnx/ONNX Runtime native libraries.")
    endif()

    set(_sherpa_include_hints
        "${SHATV_SHERPA_ONNX_ROOT}/include"
        "${SHATV_SHERPA_ONNX_ROOT}"
    )
    set(_sherpa_library_hints
        "${SHATV_SHERPA_ONNX_ROOT}/lib"
        "${SHATV_SHERPA_ONNX_ROOT}/lib64"
        "${SHATV_SHERPA_ONNX_ROOT}/bin"
        "${SHATV_SHERPA_ONNX_ROOT}"
    )

    find_path(SHATV_SHERPA_ONNX_INCLUDE_DIR
        NAMES sherpa-onnx/c-api/c-api.h
        HINTS ${_sherpa_include_hints}
        NO_DEFAULT_PATH
    )

    find_library(SHATV_SHERPA_ONNX_C_API_LIBRARY
        NAMES sherpa-onnx-c-api libsherpa-onnx-c-api
        HINTS ${_sherpa_library_hints}
        NO_DEFAULT_PATH
    )

    set(_onnxruntime_library_hints ${_sherpa_library_hints})
    if(SHATV_ONNXRUNTIME_ROOT)
        list(APPEND _onnxruntime_library_hints
            "${SHATV_ONNXRUNTIME_ROOT}/lib"
            "${SHATV_ONNXRUNTIME_ROOT}/lib64"
            "${SHATV_ONNXRUNTIME_ROOT}/bin"
            "${SHATV_ONNXRUNTIME_ROOT}"
        )
    endif()

    find_library(SHATV_ONNXRUNTIME_LIBRARY
        NAMES onnxruntime libonnxruntime
        HINTS ${_onnxruntime_library_hints}
        NO_DEFAULT_PATH
    )

    set(_missing_items "")
    if(NOT SHATV_SHERPA_ONNX_INCLUDE_DIR)
        list(APPEND _missing_items "sherpa-onnx/c-api/c-api.h under SHATV_SHERPA_ONNX_ROOT/include")
    endif()
    if(NOT SHATV_SHERPA_ONNX_C_API_LIBRARY)
        list(APPEND _missing_items "sherpa-onnx C API library under SHATV_SHERPA_ONNX_ROOT/lib or bin")
    endif()
    if(NOT SHATV_ONNXRUNTIME_LIBRARY)
        list(APPEND _missing_items "ONNX Runtime library under SHATV_SHERPA_ONNX_ROOT or SHATV_ONNXRUNTIME_ROOT")
    endif()

    if(_missing_items)
        string(REPLACE ";" "\n  - " _missing_message "${_missing_items}")
        message(FATAL_ERROR
            "SHATV_ENABLE_ASR=ON could not find required ASR dependency files:\n"
            "  - ${_missing_message}\n"
            "Set SHATV_SHERPA_ONNX_ROOT to an extracted sherpa-onnx SDK. If ONNX Runtime "
            "is packaged separately, also set SHATV_ONNXRUNTIME_ROOT. ShaTV intentionally "
            "does not require AUR packages and does not FetchContent native ASR dependencies.")
    endif()

    add_library(shatv_sherpa_onnx INTERFACE)
    target_include_directories(shatv_sherpa_onnx INTERFACE "${SHATV_SHERPA_ONNX_INCLUDE_DIR}")
    target_link_libraries(shatv_sherpa_onnx INTERFACE
        "${SHATV_SHERPA_ONNX_C_API_LIBRARY}"
        "${SHATV_ONNXRUNTIME_LIBRARY}"
    )

    get_filename_component(_sherpa_c_dir "${SHATV_SHERPA_ONNX_C_API_LIBRARY}" DIRECTORY)
    get_filename_component(_onnxruntime_dir "${SHATV_ONNXRUNTIME_LIBRARY}" DIRECTORY)
    set(SHATV_ASR_RUNTIME_LIBRARY_DIRS
        "${_sherpa_c_dir}"
        "${_onnxruntime_dir}"
        CACHE INTERNAL "Runtime library directories needed by ASR probe targets"
    )

    message(STATUS "ShaTV ASR probe enabled with sherpa-onnx root: ${SHATV_SHERPA_ONNX_ROOT}")
    set(${out_target} shatv_sherpa_onnx PARENT_SCOPE)
endfunction()
