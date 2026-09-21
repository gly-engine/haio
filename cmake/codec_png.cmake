# png reads with wuffs and writes with libspng, so each side switches on its own.
# detect.cpp needs neither and stays whichever way these go -- recognising a png is
# still useful in a build that cannot open one.
#
# expects: HAIO_CODEC_DIR, HAIO_CODEC_SOURCES_THIS
# may drop sources, and appends to HAIO_CODEC_LIBRARIES

option(HAIO_USE_SPNG "encode png with libspng" ON)

include("${CMAKE_CURRENT_LIST_DIR}/wuffs.cmake")

if(HAIO_USE_WUFFS)
    set_source_files_properties("${HAIO_CODEC_DIR}/decode.cpp" PROPERTIES
        INCLUDE_DIRECTORIES "${WUFFS_DIR}/release/c")
else()
    list(REMOVE_ITEM HAIO_CODEC_SOURCES_THIS "${HAIO_CODEC_DIR}/decode.cpp")
endif()

if(HAIO_USE_SPNG)
    set(SPNG_VERSION "v0.7.4")
    set(SPNG_DIR "${CMAKE_SOURCE_DIR}/vendor/spng")
    set(SPNG_DOWNLOAD "https://github.com/randy408/libspng/archive/refs/tags/${SPNG_VERSION}.tar.gz")
    set(SPNG_SHARED OFF CACHE BOOL "" FORCE)
    set(SPNG_STATIC ON CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    haio_fetch(spng "${SPNG_DOWNLOAD}" "${SPNG_DIR}" CMakeLists.txt)
    add_subdirectory("${SPNG_DIR}" "${CMAKE_BINARY_DIR}/spng" EXCLUDE_FROM_ALL)

    list(APPEND HAIO_CODEC_LIBRARIES spng_static)
else()
    list(REMOVE_ITEM HAIO_CODEC_SOURCES_THIS "${HAIO_CODEC_DIR}/encode.cpp")
endif()
