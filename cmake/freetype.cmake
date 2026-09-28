# text is drawn with freetype, built from source as a static library with none of its
# optional dependencies: outlines in, coverage out, and nothing compressed on the way.
#
# expects: haio_fetch
# appends: HAIO_GENERATORS_OFF when switched off, which scaffold.cmake leaves out

option(HAIO_USE_FREETYPE "draw text with freetype" ON)

set(HAIO_FREETYPE_BRUSHES "${CMAKE_SOURCE_DIR}/library/backend/codecs/generators/text")

if(NOT HAIO_USE_FREETYPE)
    foreach(dir IN LISTS HAIO_FREETYPE_BRUSHES)
        file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${dir}/*.cpp")
        set_source_files_properties(${sources} PROPERTIES HEADER_FILE_ONLY ON)
        list(APPEND HAIO_GENERATORS_OFF ${sources})
    endforeach()
    return()
endif()

set(FREETYPE_VERSION "VER-2-13-3")
set(FREETYPE_DIR "${CMAKE_SOURCE_DIR}/vendor/freetype")
set(FREETYPE_DOWNLOAD "https://github.com/freetype/freetype/archive/refs/tags/${FREETYPE_VERSION}.tar.gz")
haio_fetch(freetype "${FREETYPE_DOWNLOAD}" "${FREETYPE_DIR}" include/freetype/freetype.h)

set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
add_subdirectory("${FREETYPE_DIR}" "${CMAKE_BINARY_DIR}/freetype" EXCLUDE_FROM_ALL)

target_link_libraries(${PROJECT_NAME} PRIVATE freetype)

# the font it draws in when nobody names one, built into the binary so text works on
# a machine with no fonts at all, as the scratch image is
set(NOTO_SANS_DIR "${CMAKE_SOURCE_DIR}/vendor/fonts/noto_sans")
set(NOTO_SANS_DOWNLOAD "https://github.com/gly-engine/archive/archive/refs/heads/fonts.tar.gz")
set(NOTO_SANS_FONT "${NOTO_SANS_DIR}/Noto_Sans/NotoSans-Regular.ttf")
set(NOTO_SANS_HEADER "${CMAKE_BINARY_DIR}/include/haio/generated/fonts/noto_sans.h")
haio_fetch(noto_sans "${NOTO_SANS_DOWNLOAD}" "${NOTO_SANS_DIR}" Noto_Sans/NotoSans-Regular.ttf)

add_custom_command(
    OUTPUT "${NOTO_SANS_HEADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/include/haio/generated/fonts"
    COMMAND $<TARGET_FILE:xxd> "${NOTO_SANS_FONT}" > "${NOTO_SANS_HEADER}"
    DEPENDS xxd "${NOTO_SANS_FONT}"
    COMMENT "embedding the default font"
)
add_custom_target(haio_fonts DEPENDS "${NOTO_SANS_HEADER}")
add_dependencies(${PROJECT_NAME} haio_fonts)
