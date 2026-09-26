# THIS IS A SUPER TRASH IA GENERATE SLOPED CMAKE
cmake_minimum_required(VERSION 3.19)
include(FetchContent)
include(ExternalProject)
include(CheckCXXCompilerFlag)

project("haio" LANGUAGES C CXX)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

option(HAIO_STATIC "link the binary against nothing at all" ON)

file(GLOB_RECURSE HAIO_LIB_SOURCES "${CMAKE_CURRENT_LIST_DIR}/library/*.cpp")
add_executable(${PROJECT_NAME} "${CMAKE_CURRENT_LIST_DIR}/source/main.cpp;${HAIO_LIB_SOURCES}")
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include("${CMAKE_CURRENT_LIST_DIR}/cmake/fetch.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/boost.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/cpp26.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/redis.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/static.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/wolfssl.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/codec_gif.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/codec_jpeg.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/codec_png.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/color_etc1.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/color_yuv.cmake")

find_package(Threads REQUIRED)

add_executable(xxd "${CMAKE_CURRENT_LIST_DIR}/scripts/xxd.cpp")
add_executable(replace "${CMAKE_CURRENT_LIST_DIR}/scripts/replace.cpp")

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_BINARY_DIR}/include"
    "${WOLFSSL_DIR}/wolfssl"
    "${CMAKE_CURRENT_LIST_DIR}/vendor/etcpak"
    "${CMAKE_CURRENT_LIST_DIR}/vendor/wuffs/release/c"
    "${CMAKE_CURRENT_LIST_DIR}/vendor/boost/libs/variant/include/"
    "${CMAKE_CURRENT_LIST_DIR}/vendor/boost/libs/spirit/include/"
)
