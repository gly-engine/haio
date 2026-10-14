set(HAIO_GENERATED_DIR "${CMAKE_BINARY_DIR}/include/haio/generated")
set(HAIO_FORMATS_HEADER "${CMAKE_SOURCE_DIR}/include/haio/domain.hpp")

add_executable(gen_scaffold "${CMAKE_SOURCE_DIR}/scripts/gen_scaffold.cpp")

# codecs: every Detect/Decode/Encode/Move/Convert/Generate a codec source specialises,
# and the settings each one reads, declared in include/haio/implementation/codecs/
# and, for the brushes, include/haio/implementation/codecs/generators/
file(GLOB_RECURSE HAIO_CODEC_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/library/implementation/codecs/*.cpp")
# a brush whose dependency is switched off keeps its declaration and loses its colours
if(HAIO_GENERATORS_OFF)
    list(REMOVE_ITEM HAIO_CODEC_SOURCES ${HAIO_GENERATORS_OFF})
endif()
file(GLOB HAIO_CODEC_HEADERS CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}/include"
     "${CMAKE_SOURCE_DIR}/include/haio/implementation/codecs/*.hpp"
     "${CMAKE_SOURCE_DIR}/include/haio/implementation/codecs/generators/*.hpp")
set(HAIO_CODEC_INCLUDES --include haio/implementation/codecs.hpp)
set(HAIO_CODEC_HEADERS_ABSOLUTE "")
foreach(header IN LISTS HAIO_CODEC_HEADERS)
    list(APPEND HAIO_CODEC_INCLUDES --include "${header}")
    list(APPEND HAIO_CODEC_HEADERS_ABSOLUTE "${CMAKE_SOURCE_DIR}/include/${header}")
endforeach()
# the verbs that are classes get theirs in codec/classes.hpp, which is all a codec's own
# source includes to define its operator()
add_custom_command(
    OUTPUT "${HAIO_GENERATED_DIR}/codecs.hpp" "${HAIO_GENERATED_DIR}/codecs/classes.hpp"
    COMMAND $<TARGET_FILE:gen_scaffold> "${HAIO_GENERATED_DIR}/codecs.hpp"
            --formats "${HAIO_FORMATS_HEADER}" --namespace "Haio::Implementation::Codecs" --defaults "Haio::Implementation::WIP"
            --classes "${HAIO_GENERATED_DIR}/codecs/classes.hpp" haio/generated/codecs/classes.hpp
            ${HAIO_CODEC_INCLUDES} ${HAIO_CODEC_SOURCES}
    DEPENDS gen_scaffold "${HAIO_FORMATS_HEADER}" ${HAIO_CODEC_SOURCES} ${HAIO_CODEC_HEADERS_ABSOLUTE}
    COMMENT "scanning codecs for their capabilities"
)

# the headers in include/haio/implementation/<dir>/ and the sources in
# library/implementation/<dir>/, both found by looking, so a new one is a new file and
# nothing else: <output> includes every header and declares every specialisation
function(haio_scaffold output space dir)
    # the verbs that are classes get theirs beside it, in <output without .hpp>/classes.hpp
    string(REGEX REPLACE "\\.hpp$" "/classes.hpp" classes "${output}")
    file(GLOB headers CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}/include"
         "${CMAKE_SOURCE_DIR}/include/haio/implementation/${dir}/*.hpp")
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/library/implementation/${dir}/*.cpp")

    set(includes "")
    set(absolute "")
    foreach(header IN LISTS headers)
        list(APPEND includes --include "${header}")
        list(APPEND absolute "${CMAKE_SOURCE_DIR}/include/${header}")
    endforeach()

    add_custom_command(
        OUTPUT "${HAIO_GENERATED_DIR}/${output}" "${HAIO_GENERATED_DIR}/${classes}"
        COMMAND $<TARGET_FILE:gen_scaffold> "${HAIO_GENERATED_DIR}/${output}"
                --namespace "${space}" --classes "${HAIO_GENERATED_DIR}/${classes}" "haio/generated/${classes}"
                ${includes} ${sources}
        DEPENDS gen_scaffold ${absolute} ${sources}
        COMMENT "scanning ${dir} for what it declares"
    )
endfunction()

# transforms: the colours each one runs on, and the blends -composite mixes with
haio_scaffold(transforms.hpp "Haio::Implementation::Transforms" transforms)
# stages: what the command line can name, one header each. a builder is declared in
# its stage's header, so what this adds is only that every stage is included
haio_scaffold(language/stages.hpp "Haio::Implementation::LanguageStage" language/stages)
set(HAIO_STAGE_HEADERS "${HAIO_GENERATED_DIR}/transforms.hpp" "${HAIO_GENERATED_DIR}/language/stages.hpp")

add_custom_target(haio_scaffold DEPENDS "${HAIO_GENERATED_DIR}/codecs.hpp" ${HAIO_STAGE_HEADERS})
add_dependencies(${PROJECT_NAME} haio_scaffold)

# the doxygen INPUT_FILTER that runs the @haio{convert} examples, and the same code as
# doxygen_images, which draws them before doxygen starts; static, since doxygen may run in a container
foreach(tool doxygen_filter doxygen_images)
    add_executable(${tool} "${CMAKE_SOURCE_DIR}/scripts/doxygen_filter.cpp")
    target_link_options(${tool} PRIVATE -static)
    set_target_properties(${tool} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endforeach()

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

# every @haio{convert} picture, drawn by the haio just built; the ones already drawn are kept
add_custom_target(haio_docs_images ALL
    COMMAND $<TARGET_FILE:doxygen_images> include library
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEPENDS doxygen_images ${PROJECT_NAME}
    COMMENT "drawing the pictures the docs show"
)
