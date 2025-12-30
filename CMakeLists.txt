cmake_minimum_required(VERSION 3.19)
include(FetchContent)

project("img" LANGUAGES C)
set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*/*.c")
add_executable(${PROJECT_NAME} "src/main.c;${SOURCES}")
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

set(LIBUV_VERSION "v1.51.0")
set(LIBUV_REPO "https://github.com/libuv/libuv.git")
set(LIBUV_ROOT "${CMAKE_SOURCE_DIR}/vendor/libuv")
set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(uv_a GIT_REPOSITORY ${LIBUV_REPO} GIT_TAG ${LIBUV_VERSION} SOURCE_DIR ${LIBUV_ROOT})
FetchContent_MakeAvailable(uv_a)
target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUV_ROOT}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE uv_a)

set(ZLIB_VERSION "v1.3.1")
set(ZLIB_REPO "https://github.com/madler/zlib.git")
set(ZLIB_DIR "${CMAKE_SOURCE_DIR}/vendor/zlib")
set(ZLIB_ROOT "${ZLIB_DIR}" CACHE STRING "" FORCE)
set(ZLIB_LIBRARY "${CMAKE_SOURCE_DIR}/build/_deps/zlib-build/libz.a" CACHE FILEPATH "")
set(ZLIB_INCLUDE_DIR "${ZLIB_DIR}" CACHE PATH "")
set(ZLIB_FOUND TRUE CACHE BOOL "")
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zlib GIT_REPOSITORY ${ZLIB_REPO} GIT_TAG ${ZLIB_VERSION} SOURCE_DIR ${ZLIB_ROOT})
FetchContent_MakeAvailable(zlib)
set_target_properties(zlib PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_library(ZLIB::ZLIB ALIAS zlibstatic)

set(SPNG_VERSION "v0.7.4")
set(SPNG_REPO "https://github.com/randy408/libspng.git")
set(SPNG_ROOT "${CMAKE_SOURCE_DIR}/vendor/spng")
set(SPNG_SHARED OFF CACHE BOOL "" FORCE)
set(SPNG_STATIC ON CACHE BOOL "" FORCE)
set(SPNG_USE_MINIZ OFF CACHE BOOL "" FORCE)
set(SPNG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    spng
    GIT_REPOSITORY ${SPNG_REPO}
    GIT_TAG ${SPNG_VERSION}
    SOURCE_DIR ${SPNG_ROOT}
)
FetchContent_MakeAvailable(spng)
target_link_libraries(${PROJECT_NAME} PRIVATE spng_static)

set(KLIB_DIR "${CMAKE_SOURCE_DIR}/vendor/klib")
set(KLIB_DOWNLOAD "https://github.com/attractivechaos/klib/archive/5c1451caa1ee476624d00eed71810532c89b82d1.tar.gz")
FetchContent_Populate(dep_klib URL "${KLIB_DOWNLOAD}" SOURCE_DIR ${KLIB_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${KLIB_DIR})

set(FFMPEG_VERSION "7.0.2")
set(FFMPEG_DOWNLOAD "https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n${FFMPEG_VERSION}.tar.gz")
set(FFMPEG_DIR "${CMAKE_SOURCE_DIR}/vendor/ffmpeg")
if (NOT EXISTS "${FFMPEG_DIR}/libswscale/libswscale.a")
    FetchContent_Populate(dep_ffmpeg URL ${FFMPEG_DOWNLOAD} SOURCE_DIR ${FFMPEG_DIR})
    # todo: remove --disable-x86asm
    execute_process(
        COMMAND ./configure --prefix=${FFMPEG_DIR}/build --enable-static --disable-shared
        --disable-avdevice  --disable-avcodec --disable-avformat --disable-swresample --disable-postproc --disable-avfilter
        --disable-x86asm
        WORKING_DIRECTORY ${FFMPEG_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND ${CMAKE_MAKE_PROGRAM} CC=${CMAKE_C_COMPILER} libswscale/libswscale.a libavutil/libavutil.a
        WORKING_DIRECTORY ${FFMPEG_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE "${FFMPEG_DIR}/libswscale/libswscale.a")
target_link_libraries(${PROJECT_NAME} PRIVATE "${FFMPEG_DIR}/libavutil/libavutil.a")
target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_DIR})
