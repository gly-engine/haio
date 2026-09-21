# etc1 compresses and decompresses with etcpak, compiled into the library rather than
# linked. Unlike a container, a colour conversion lives in two files -- one per
# direction -- so the dependency is attached by name instead of by directory.
#
# expects: HAIO_COLOR_SOURCES
# appends: HAIO_CODEC_SOURCES_EXTRA

option(HAIO_USE_ETCPAK "compress and decompress etc1 with etcpak" ON)

set(HAIO_ETC1_SOURCES "")
foreach(source IN LISTS HAIO_COLOR_SOURCES)
    if(source MATCHES "etc1")
        list(APPEND HAIO_ETC1_SOURCES "${source}")
    endif()
endforeach()

if(NOT HAIO_USE_ETCPAK)
    foreach(source IN LISTS HAIO_ETC1_SOURCES)
        list(REMOVE_ITEM HAIO_COLOR_SOURCES "${source}")
        list(REMOVE_ITEM HAIO_LIB_SOURCES "${source}")
    endforeach()
    list(APPEND HAIO_CODECS_OFF "color/etc1")
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

# only the two etc1 files see etcpak's headers
set_source_files_properties(${HAIO_ETC1_SOURCES} PROPERTIES INCLUDE_DIRECTORIES "${ETCPAK_DIR}")
list(APPEND HAIO_CODEC_SOURCES_EXTRA ${ETCPAK_SOURCES})
