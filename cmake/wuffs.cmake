# wuffs is one file that is both header and implementation, and its base module carries
# data as well as functions: WUFFS_CONFIG__STATIC_FUNCTIONS makes the functions static
# and leaves every status string extern, so a second decoder defining the implementation
# collides with the first. one translation unit defines it for everyone, and the
# decoders include the same file for its declarations alone.
#
# included by each codec that decodes with wuffs, and does its work once.
#
# appends: HAIO_CODEC_SOURCES_EXTRA

option(HAIO_USE_WUFFS "decode png and gif with wuffs" ON)

set(WUFFS_VERSION "v0.3.4")
set(WUFFS_DIR "${CMAKE_SOURCE_DIR}/vendor/wuffs")

if(NOT HAIO_USE_WUFFS OR HAIO_WUFFS_READY)
    return()
endif()

set(WUFFS_DOWNLOAD "https://github.com/google/wuffs-mirror-release-c/archive/refs/tags/${WUFFS_VERSION}.tar.gz")
haio_fetch(wuffs "${WUFFS_DOWNLOAD}" "${WUFFS_DIR}" release/c/wuffs-v0.4.c)

# it is not inside a container's directory, so the codec loop never sees it, but the
# library glob does: hand it over once and take it out of the list it came from
set(WUFFS_SOURCE "${CMAKE_SOURCE_DIR}/library/backend/codecs/wuffs.cpp")
list(REMOVE_ITEM HAIO_LIB_SOURCES "${WUFFS_SOURCE}")
list(APPEND HAIO_CODEC_SOURCES_EXTRA "${WUFFS_SOURCE}")
set_source_files_properties("${WUFFS_SOURCE}" PROPERTIES INCLUDE_DIRECTORIES "${WUFFS_DIR}/release/c")

set(HAIO_WUFFS_READY ON)
