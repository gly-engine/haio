# jpeg reads and writes through libjpeg-turbo's TurboJPEG api, which is the one that
# speaks yuv planes directly. that is why it is the choice here: a jpeg is yuv all the
# way down, and a decoder that hands back rgb has already thrown away the chroma
# layout and spent the work doing it.
#
# it is the one dependency built as an external project rather than a subdirectory,
# because libjpeg-turbo refuses to be a subdirectory and says so with a fatal error.
#
# detect.cpp needs none of this and stays whichever way the switch goes: recognising a
# jpeg is still useful in a build that cannot open one.
#
# expects: HAIO_CODEC_DIR, HAIO_CODEC_SOURCES_THIS
# may drop sources, and appends to HAIO_CODEC_LIBRARIES and HAIO_CODEC_DEPENDS

option(HAIO_USE_JPEGTURBO "read and write jpeg with libjpeg-turbo" ON)

if(HAIO_USE_JPEGTURBO)
    include(ExternalProject)

    set(JPEGTURBO_VERSION "3.0.4")
    set(JPEGTURBO_DIR "${CMAKE_SOURCE_DIR}/vendor/libjpeg-turbo")
    set(JPEGTURBO_BIN "${CMAKE_BINARY_DIR}/libjpeg-turbo")
    set(JPEGTURBO_DOWNLOAD "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/${JPEGTURBO_VERSION}.tar.gz")

    haio_fetch(libjpeg-turbo "${JPEGTURBO_DOWNLOAD}" "${JPEGTURBO_DIR}" CMakeLists.txt)
    file(MAKE_DIRECTORY "${JPEGTURBO_BIN}/include")

    ExternalProject_Add(libjpeg_turbo_proj
        SOURCE_DIR "${JPEGTURBO_DIR}"
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${JPEGTURBO_BIN}
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DENABLE_SHARED=OFF
            -DENABLE_STATIC=ON
            -DWITH_TURBOJPEG=ON
        UPDATE_COMMAND ""
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target turbojpeg-static
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E make_directory ${JPEGTURBO_BIN}/lib
            COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/libturbojpeg.a ${JPEGTURBO_BIN}/lib/libturbojpeg.a
            COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/turbojpeg.h ${JPEGTURBO_BIN}/include/turbojpeg.h
        BUILD_BYPRODUCTS ${JPEGTURBO_BIN}/lib/libturbojpeg.a
    )

    add_library(turbojpeg STATIC IMPORTED)
    set_target_properties(turbojpeg PROPERTIES
        IMPORTED_LOCATION "${JPEGTURBO_BIN}/lib/libturbojpeg.a"
        INTERFACE_INCLUDE_DIRECTORIES "${JPEGTURBO_BIN}/include"
    )

    list(APPEND HAIO_CODEC_LIBRARIES turbojpeg)
    # an imported library carries no build order of its own, so whoever links it has
    # to be told to wait for the project that produces the file
    list(APPEND HAIO_CODEC_DEPENDS libjpeg_turbo_proj)
else()
    list(REMOVE_ITEM HAIO_CODEC_SOURCES_THIS "${HAIO_CODEC_DIR}/decode.cpp")
    list(REMOVE_ITEM HAIO_CODEC_SOURCES_THIS "${HAIO_CODEC_DIR}/encode.cpp")
endif()
