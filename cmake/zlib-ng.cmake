set(ZLIBNG_VERSION "2.2.5")
set(ZLIBNG_DIR "${CMAKE_SOURCE_DIR}/vendor/zlib-ng")
set(ZLIBNG_DOWNLOAD "https://github.com/zlib-ng/zlib-ng/archive/refs/tags/${ZLIBNG_VERSION}.tar.gz")

set(ZLIB_COMPAT ON CACHE BOOL "" FORCE)
set(ZLIB_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ZLIBNG_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(WITH_GTEST OFF CACHE BOOL "" FORCE)
set(WITH_GZFILEOP ON CACHE BOOL "" FORCE)

haio_fetch(zlib-ng "${ZLIBNG_DOWNLOAD}" "${ZLIBNG_DIR}" CMakeLists.txt)

add_subdirectory("${ZLIBNG_DIR}" "${CMAKE_BINARY_DIR}/zlib-ng" EXCLUDE_FROM_ALL)

target_link_libraries(${PROJECT_NAME} PRIVATE zlib)