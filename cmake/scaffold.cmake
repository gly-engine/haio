set(HAIO_GENERATED_DIR "${CMAKE_BINARY_DIR}/include/haio/generated")
set(HAIO_FORMATS_HEADER "${CMAKE_SOURCE_DIR}/include/haio_formats.hpp")

add_executable(gen_scaffold "${CMAKE_SOURCE_DIR}/scripts/gen_scaffold.cpp")

# codecs: every Detect/Decode/Encode/Move/Convert/Generate a codec source specialises,
# and the settings each one reads, declared in include/haio/codecs/ and, for the
# brushes, include/haio/codecs/generators/
file(GLOB_RECURSE HAIO_CODEC_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/library/backend/codecs/*.cpp")
file(GLOB HAIO_CODEC_HEADERS CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}/include"
     "${CMAKE_SOURCE_DIR}/include/haio/codecs/*.hpp" "${CMAKE_SOURCE_DIR}/include/haio/codecs/generators/*.hpp")
set(HAIO_CODEC_INCLUDES --include haio_codec.hpp)
set(HAIO_CODEC_HEADERS_ABSOLUTE "")
foreach(header IN LISTS HAIO_CODEC_HEADERS)
    list(APPEND HAIO_CODEC_INCLUDES --include "${header}")
    list(APPEND HAIO_CODEC_HEADERS_ABSOLUTE "${CMAKE_SOURCE_DIR}/include/${header}")
endforeach()
add_custom_command(
    OUTPUT "${HAIO_GENERATED_DIR}/codec.hpp"
    COMMAND $<TARGET_FILE:gen_scaffold> "${HAIO_GENERATED_DIR}/codec.hpp"
            --formats "${HAIO_FORMATS_HEADER}" ${HAIO_CODEC_INCLUDES} ${HAIO_CODEC_SOURCES}
    DEPENDS gen_scaffold "${HAIO_FORMATS_HEADER}" ${HAIO_CODEC_SOURCES} ${HAIO_CODEC_HEADERS_ABSOLUTE}
    COMMENT "scanning codecs for their capabilities"
)

# transforms: a header in include/haio/<kind>/ declares the stage the
# command line reads, a source in library/backend/<kind>/ is one colour it runs on.
# both are found by looking, so a new one is a new file and nothing else.
set(HAIO_STAGE_HEADERS "")
foreach(kind transforms)
    string(REGEX REPLACE "s$" "" single "${kind}")
    string(SUBSTRING "${single}" 0 1 first)
    string(TOUPPER "${first}" first)
    string(SUBSTRING "${single}" 1 -1 rest)

    file(GLOB headers CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}/include"
         "${CMAKE_SOURCE_DIR}/include/haio/${kind}/*.hpp")
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/library/backend/${kind}/*.cpp")

    set(includes "")
    set(absolute "")
    foreach(header IN LISTS headers)
        list(APPEND includes --include "${header}")
        list(APPEND absolute "${CMAKE_SOURCE_DIR}/include/${header}")
    endforeach()

    add_custom_command(
        OUTPUT "${HAIO_GENERATED_DIR}/${single}.hpp"
        COMMAND $<TARGET_FILE:gen_scaffold> "${HAIO_GENERATED_DIR}/${single}.hpp"
                --namespace "Haio::${first}${rest}s" ${includes} ${sources}
        DEPENDS gen_scaffold ${absolute} ${sources}
        COMMENT "scanning ${kind} for the colours they run on"
    )
    list(APPEND HAIO_STAGE_HEADERS "${HAIO_GENERATED_DIR}/${single}.hpp")
endforeach()

add_custom_target(haio_scaffold DEPENDS "${HAIO_GENERATED_DIR}/codec.hpp" ${HAIO_STAGE_HEADERS})
add_dependencies(${PROJECT_NAME} haio_scaffold)

# the convert grammar, printed to stdout for whoever documents it
add_executable(generate_ebnf "${CMAKE_SOURCE_DIR}/scripts/generate_ebnf.cpp")
target_compile_options(generate_ebnf PRIVATE -std=c++26 -freflection)
target_include_directories(generate_ebnf PRIVATE "${CMAKE_SOURCE_DIR}/include" "${CMAKE_BINARY_DIR}/include")
set_target_properties(generate_ebnf PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_dependencies(generate_ebnf haio_scaffold)

# and the page doxygen reads it from, which every build writes: a page in the convert
# topic, next to the code it documents rather than a copy in docs/ that can go stale
set(HAIO_EBNF_PAGE "${CMAKE_BINARY_DIR}/docs/ebnf.md")
add_custom_command(
    OUTPUT "${HAIO_EBNF_PAGE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/docs"
    COMMAND $<TARGET_FILE:generate_ebnf> > "${HAIO_EBNF_PAGE}"
    DEPENDS generate_ebnf
    COMMENT "printing the convert grammar"
)
add_custom_target(haio_docs ALL DEPENDS "${HAIO_EBNF_PAGE}")
