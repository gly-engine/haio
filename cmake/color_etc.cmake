# the etc family -- etc1, etc2, etc2 with an eac alpha, and the one and two channel
# eacs -- compresses and decompresses with etcpak, compiled into the library rather
# than linked. Unlike a container, a colour conversion lives in two files -- one per
# direction -- so the dependency is attached by name instead of by directory.
#
# expects: HAIO_COLOR_SOURCES
# appends: HAIO_CODEC_SOURCES_EXTRA

option(HAIO_USE_ETCPAK "compress and decompress the etc colours with etcpak" ON)

set(HAIO_ETC_SOURCES "")
foreach(source IN LISTS HAIO_COLOR_SOURCES)
    if(source MATCHES "etc1|etc2|eac_r")
        list(APPEND HAIO_ETC_SOURCES "${source}")
    endif()
endforeach()

if(NOT HAIO_USE_ETCPAK)
    foreach(source IN LISTS HAIO_ETC_SOURCES)
        list(REMOVE_ITEM HAIO_COLOR_SOURCES "${source}")
        list(REMOVE_ITEM HAIO_LIB_SOURCES "${source}")
    endforeach()
    list(APPEND HAIO_CODECS_OFF "color/etc1" "color/etc2" "color/etc2a" "color/eac_r11" "color/eac_rg11")
    return()
endif()

set(ETCPAK_VERSION "2.1")
set(ETCPAK_DIR "${CMAKE_SOURCE_DIR}/vendor/etcpak")
set(ETCPAK_DOWNLOAD "https://github.com/wolfpld/etcpak/archive/refs/tags/${ETCPAK_VERSION}.tar.gz")
haio_fetch(etcpak "${ETCPAK_DOWNLOAD}" "${ETCPAK_DIR}" ProcessRGB.cpp)

set(ETCPAK_SOURCES
    "${ETCPAK_DIR}/bcdec.c"
    "${ETCPAK_DIR}/Decode.cpp"
    "${ETCPAK_DIR}/Dither.cpp"
    "${ETCPAK_DIR}/ProcessRGB.cpp"
    "${ETCPAK_DIR}/Tables.cpp"
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64|i[3-6]86)$")
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-msse4.1" HAIO_HAS_SSE41)
    if(HAIO_HAS_SSE41)
        set_source_files_properties(${ETCPAK_SOURCES} PROPERTIES COMPILE_OPTIONS "-msse4.1")
    endif()
endif()

set_source_files_properties(${HAIO_ETC_SOURCES} PROPERTIES INCLUDE_DIRECTORIES "${ETCPAK_DIR}")
add_library(etc "${ETCPAK_SOURCES}")
target_link_libraries(${PROJECT_NAME} PRIVATE etc)
