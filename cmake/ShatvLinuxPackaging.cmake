if(NOT UNIX OR APPLE)
    return()
endif()

set(SHATV_LINUX_PACKAGE_DISTRIBUTION "ubuntu-24.04" CACHE STRING "Linux distribution label used in package asset names")
set(SHATV_LINUX_PACKAGE_CONTACT "MaxCrazy <alex02newton@gmail.com>" CACHE STRING "Linux package maintainer/contact")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(_shatv_deb_arch "amd64")
else()
    set(_shatv_deb_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(SHATV_ENABLE_ASR)
    set(_shatv_deb_package_name "shatv-asr")
    set(_shatv_asset_suffix "-asr")
    set(_shatv_conflicting_package "shatv")
    set(_shatv_package_summary "ShaTV IPTV player with ASR runtime support")
    set(_shatv_package_description
        "ShaTV is a cross-platform IPTV player. This ASR-capable package includes native speech-recognition runtime libraries but does not include ASR model files.")
    set(_shatv_extra_notice
"ASR package additions:
- sherpa-onnx runtime libraries
- ONNX Runtime libraries required by sherpa-onnx
- libarchive as a system dependency for ASR model archive extraction
")
    set(_shatv_extra_sources
"## sherpa-onnx
- Binary source: ASR package build input
- Upstream source: https://github.com/k2-fsa/sherpa-onnx

## ONNX Runtime
- Bundled with the selected sherpa-onnx binary package.
- Upstream source: https://github.com/microsoft/onnxruntime

## libarchive
- System dependency on Debian/Ubuntu.
- Upstream source: https://github.com/libarchive/libarchive
")
else()
    set(_shatv_deb_package_name "shatv")
    set(_shatv_asset_suffix "")
    set(_shatv_conflicting_package "shatv-asr")
    set(_shatv_package_summary "ShaTV IPTV player")
    set(_shatv_package_description
        "ShaTV is a cross-platform IPTV player.")
    set(_shatv_extra_notice "")
    set(_shatv_extra_sources "")
endif()

set(SHATV_DEBIAN_PACKAGE_NAME "${_shatv_deb_package_name}" CACHE STRING "Debian binary package name")
set(SHATV_LINUX_PACKAGE_FILE_NAME
    "shatv-${SHATV_LINUX_PACKAGE_DISTRIBUTION}-${_shatv_deb_arch}${_shatv_asset_suffix}"
    CACHE STRING "Linux package asset file name without extension")

configure_file(
    ${CMAKE_SOURCE_DIR}/packaging/linux/top.shanana.shatv.desktop.in
    ${CMAKE_BINARY_DIR}/packaging/linux/top.shanana.shatv.desktop
    @ONLY
)
configure_file(
    ${CMAKE_SOURCE_DIR}/packaging/linux/top.shanana.shatv.metainfo.xml.in
    ${CMAKE_BINARY_DIR}/packaging/linux/top.shanana.shatv.metainfo.xml
    @ONLY
)

set(SHATV_LINUX_PACKAGE_EXTRA_NOTICE "${_shatv_extra_notice}")
set(SHATV_LINUX_PACKAGE_EXTRA_SOURCES "${_shatv_extra_sources}")
configure_file(
    ${CMAKE_SOURCE_DIR}/packaging/linux/NOTICE.txt.in
    ${CMAKE_BINARY_DIR}/packaging/linux/NOTICE.txt
    @ONLY
)
configure_file(
    ${CMAKE_SOURCE_DIR}/packaging/linux/THIRD_PARTY_SOURCES.md.in
    ${CMAKE_BINARY_DIR}/packaging/linux/THIRD_PARTY_SOURCES.md
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/packaging/linux/top.shanana.shatv.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES ${CMAKE_BINARY_DIR}/packaging/linux/top.shanana.shatv.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)
install(FILES ${CMAKE_SOURCE_DIR}/packaging/linux/icons/hicolor/scalable/apps/shatv.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
install(FILES
        ${CMAKE_BINARY_DIR}/packaging/linux/NOTICE.txt
        ${CMAKE_BINARY_DIR}/packaging/linux/THIRD_PARTY_SOURCES.md
        ${CMAKE_SOURCE_DIR}/README.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/${SHATV_DEBIAN_PACKAGE_NAME}
)
install(FILES
        ${CMAKE_SOURCE_DIR}/packaging/licenses/LGPL-2.1.txt
        ${CMAKE_SOURCE_DIR}/packaging/licenses/LGPL-3.0.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/${SHATV_DEBIAN_PACKAGE_NAME}/licenses
)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "${SHATV_DEBIAN_PACKAGE_NAME}")
set(CPACK_PACKAGE_VENDOR "MaxCrazy")
set(CPACK_PACKAGE_CONTACT "${SHATV_LINUX_PACKAGE_CONTACT}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${_shatv_package_summary}")
set(CPACK_PACKAGE_DESCRIPTION "${_shatv_package_description}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${SHATV_LINUX_PACKAGE_FILE_NAME}")
set(CPACK_DEBIAN_FILE_NAME "${SHATV_LINUX_PACKAGE_FILE_NAME}.deb")
set(CPACK_DEBIAN_PACKAGE_NAME "${SHATV_DEBIAN_PACKAGE_NAME}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${SHATV_LINUX_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${_shatv_deb_arch}")
set(CPACK_DEBIAN_PACKAGE_SECTION "video")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/MaxCrazy1101/shatv")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-dialogs, qml6-module-qtquick-layouts, qml6-module-qtquick-window, qt6-qpa-plugins"
)
set(CPACK_DEBIAN_PACKAGE_BREAKS "${_shatv_conflicting_package}")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "${_shatv_conflicting_package}")
set(CPACK_DEBIAN_PACKAGE_REPLACES "${_shatv_conflicting_package}")

if(SHATV_ENABLE_ASR AND SHATV_INSTALL_BUNDLED_ASR_RUNTIME)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${CMAKE_INSTALL_FULL_LIBDIR}/shatv")
endif()

include(CPack)
