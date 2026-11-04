# webp reads and writes through libwebp, both bitstreams in one library, so one switch
# covers both sides. libwebp.a leaves sharpyuv undefined, so the two travel together.
#
# expects: HAIO_CODEC_DIR, HAIO_CODEC_SOURCES_THIS
# may drop sources, and appends to HAIO_CODEC_LIBRARIES

option(HAIO_USE_LIBWEBP "read and write webp with libwebp" ON)

if(HAIO_USE_LIBWEBP)
    include(ExternalProject)

    set(LIBWEBP_VERSION "1.5.0")
    set(LIBWEBP_DIR "${CMAKE_SOURCE_DIR}/vendor/libwebp")
    set(LIBWEBP_BIN "${CMAKE_BINARY_DIR}/libwebp")
    set(LIBWEBP_DOWNLOAD "https://github.com/webmproject/libwebp/archive/refs/tags/v${LIBWEBP_VERSION}.tar.gz")

    haio_fetch(libwebp "${LIBWEBP_DOWNLOAD}" "${LIBWEBP_DIR}" CMakeLists.txt)
    file(MAKE_DIRECTORY "${LIBWEBP_BIN}/include/webp")

    ExternalProject_Add(libwebp_proj
        SOURCE_DIR "${LIBWEBP_DIR}"
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${LIBWEBP_BIN}
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DBUILD_SHARED_LIBS=OFF
            -DWEBP_BUILD_ANIM_UTILS=OFF
            -DWEBP_BUILD_CWEBP=OFF
            -DWEBP_BUILD_DWEBP=OFF
            -DWEBP_BUILD_GIF2WEBP=OFF
            -DWEBP_BUILD_IMG2WEBP=OFF
            -DWEBP_BUILD_VWEBP=OFF
            -DWEBP_BUILD_WEBPINFO=OFF
            -DWEBP_BUILD_WEBPMUX=OFF
            -DWEBP_BUILD_EXTRAS=OFF
            -DWEBP_BUILD_LIBWEBPMUX=OFF
            -DWEBP_USE_THREAD=OFF
        UPDATE_COMMAND ""
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target webp
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E make_directory ${LIBWEBP_BIN}/lib
            COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/libwebp.a ${LIBWEBP_BIN}/lib/libwebp.a
            COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/libsharpyuv.a ${LIBWEBP_BIN}/lib/libsharpyuv.a
            COMMAND ${CMAKE_COMMAND} -E copy
                <SOURCE_DIR>/src/webp/decode.h
                <SOURCE_DIR>/src/webp/encode.h
                <SOURCE_DIR>/src/webp/types.h
                ${LIBWEBP_BIN}/include/webp/
        BUILD_BYPRODUCTS
            ${LIBWEBP_BIN}/lib/libwebp.a
            ${LIBWEBP_BIN}/lib/libsharpyuv.a
    )

    add_library(sharpyuv STATIC IMPORTED)
    set_target_properties(sharpyuv PROPERTIES
        IMPORTED_LOCATION "${LIBWEBP_BIN}/lib/libsharpyuv.a"
    )

    add_library(webp STATIC IMPORTED)
    set_target_properties(webp PROPERTIES
        IMPORTED_LOCATION "${LIBWEBP_BIN}/lib/libwebp.a"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBWEBP_BIN}/include"
        INTERFACE_LINK_LIBRARIES sharpyuv
    )

    target_include_directories(${PROJECT_NAME} PRIVATE "${LIBWEBP_BIN}/include")

    add_dependencies(webp libwebp_proj)
    add_dependencies(${PROJECT_NAME} libwebp_proj)
    target_link_libraries(${PROJECT_NAME} PRIVATE webp)

    list(APPEND HAIO_CODEC_LIBRARIES webp)
else()
    list(REMOVE_ITEM HAIO_CODEC_SOURCES_THIS "${HAIO_CODEC_DIR}/decode.cpp")
    list(REMOVE_ITEM HAIO_CODEC_SOURCES_THIS "${HAIO_CODEC_DIR}/encode.cpp")
endif()
