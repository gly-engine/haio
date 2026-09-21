# libspng calls find_package(ZLIB REQUIRED) the ordinary way, which would reach for
# whatever the system carries. This module sits earlier on CMAKE_MODULE_PATH and hands
# it the zlib-ng already built here instead, so one deflate serves the whole binary.
#
# It is deliberately not a real search: there is nothing to look for, the answer is
# already a target in this build.

# zlibstatic is itself an alias in this configuration, and cmake will not alias an
# alias, so the real target is the one named here
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlib)
endif()

set(ZLIB_FOUND TRUE)
set(ZLIB_LIBRARIES ZLIB::ZLIB)
set(ZLIB_INCLUDE_DIRS "$<TARGET_PROPERTY:zlib,INTERFACE_INCLUDE_DIRECTORIES>")
