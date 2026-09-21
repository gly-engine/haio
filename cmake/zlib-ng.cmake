# One deflate for the whole binary, and a faster one than the original.
#
# Before this there were two: libspng asked the system for ZLIB and got whatever the
# machine happened to carry, while the zip reader used a copy in vendor/. Both ended
# up linked, from different versions, neither of them chosen.
#
# zlib-ng in compat mode keeps zlib's own API and ABI, so nothing that uses it has to
# know. That matters here because libspng inflates with the streaming z_stream calls:
# libdeflate would be faster still for the zip reader, which inflates whole buffers in
# one go, but it has no streaming API at all and spng could not use it.
#
# cmake/FindZLIB.cmake hands spng the target built here, so its find_package(ZLIB)
# resolves inside the build instead of on the system.

set(ZLIBNG_VERSION "2.2.5")
set(ZLIBNG_DIR "${CMAKE_SOURCE_DIR}/vendor/zlib-ng")
set(ZLIBNG_DOWNLOAD "https://github.com/zlib-ng/zlib-ng/archive/refs/tags/${ZLIBNG_VERSION}.tar.gz")

# compat is what makes it a drop in: the headers and symbols stay zlib's
set(ZLIB_COMPAT ON CACHE BOOL "" FORCE)
set(ZLIB_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ZLIBNG_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(WITH_GTEST OFF CACHE BOOL "" FORCE)
set(WITH_GZFILEOP ON CACHE BOOL "" FORCE)

haio_fetch(zlib-ng "${ZLIBNG_DOWNLOAD}" "${ZLIBNG_DIR}" CMakeLists.txt)

# BUILD_SHARED_LIBS is off for the whole project, set before any subdirectory, so the
# static target is the one zlib-ng creates here
add_subdirectory("${ZLIBNG_DIR}" "${CMAKE_BINARY_DIR}/zlib-ng" EXCLUDE_FROM_ALL)
