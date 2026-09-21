# gif reads with wuffs and does not write, so the switch covers decode alone.
# detect.cpp needs nothing and stays either way.
#
# expects: HAIO_CODEC_DIR, HAIO_CODEC_SOURCES_THIS
# may drop sources

include("${CMAKE_CURRENT_LIST_DIR}/wuffs.cmake")

if(HAIO_USE_WUFFS)
    set_source_files_properties("${HAIO_CODEC_DIR}/decode.cpp" PROPERTIES
        INCLUDE_DIRECTORIES "${WUFFS_DIR}/release/c")
else()
    list(REMOVE_ITEM HAIO_CODEC_SOURCES_THIS "${HAIO_CODEC_DIR}/decode.cpp")
endif()
