option(HAIO_USE_LIBYUV "convert images with libyuv" ON)

set(LIBYUV_VERSION "a7c0e17c5aaefcbe6b0c35e17fa0a83727370f0a")
set(LIBYUV_DIR "${CMAKE_SOURCE_DIR}/vendor/libyuv")
set(LIBYUV_BIN "${CMAKE_BINARY_DIR}/libyuv")
set(LIBYUV_DOWNLOAD "https://chromium.googlesource.com/libyuv/libyuv/+archive/${LIBYUV_VERSION}.tar.gz")

haio_fetch(libyuv "${LIBYUV_DOWNLOAD}" "${LIBYUV_DIR}" CMakeLists.txt)
file(MAKE_DIRECTORY "${LIBYUV_BIN}/include")

ExternalProject_Add(libyuv_proj
    SOURCE_DIR "${LIBYUV_DIR}"
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${LIBYUV_BIN}
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DBUILD_SHARED_LIBS=OFF
        -DUNIT_TEST=OFF
    UPDATE_COMMAND ""
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target yuv
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory ${LIBYUV_BIN}/lib
        COMMAND ${CMAKE_COMMAND} -E make_directory ${LIBYUV_BIN}/include
        COMMAND ${CMAKE_COMMAND} -E copy
            <BINARY_DIR>/libyuv.a
            ${LIBYUV_BIN}/lib/libyuv.a
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            <SOURCE_DIR>/include/libyuv
            ${LIBYUV_BIN}/include/libyuv
    BUILD_BYPRODUCTS ${LIBYUV_BIN}/lib/libyuv.a
)

add_library(libyuv STATIC IMPORTED)
set_target_properties(libyuv PROPERTIES
    IMPORTED_LOCATION "${LIBYUV_BIN}/lib/libyuv.a"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBYUV_BIN}/include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE libyuv)
target_include_directories(${PROJECT_NAME} PRIVATE "${LIBYUV_DIR}/include")
