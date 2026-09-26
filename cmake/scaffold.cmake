file(GLOB_RECURSE HAIO_CODEC_SOURCES "${CMAKE_SOURCE_DIR}/library/backend/codecs/*.cpp")
set(HAIO_CODECS_HEADER "${CMAKE_BINARY_DIR}/include/haio_codecs.hpp")
set(HAIO_FORMATS_HEADER "${CMAKE_SOURCE_DIR}/include/haio_formats.hpp")

add_executable(gen_codecs "${CMAKE_SOURCE_DIR}/scripts/gen_codecs.cpp")
add_custom_command(
    OUTPUT "${HAIO_CODECS_HEADER}"
    COMMAND $<TARGET_FILE:gen_codecs> "${HAIO_CODECS_HEADER}"
            --formats "${HAIO_FORMATS_HEADER}" ${HAIO_CODEC_SOURCES}
    DEPENDS gen_codecs "${HAIO_FORMATS_HEADER}" ${HAIO_CODEC_SOURCES}
    COMMENT "scanning codecs for their capabilities"
)
add_custom_target(haio_codecs DEPENDS "${HAIO_CODECS_HEADER}")
add_dependencies(${PROJECT_NAME} haio_codecs)
