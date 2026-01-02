cmake_minimum_required(VERSION 3.19)
include(FetchContent)

project("img" LANGUAGES C)
set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*/*.c")
add_executable(${PROJECT_NAME} "src/main.c;${SOURCES}")
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/raio-gen)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(${PROJECT_NAME}  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

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
target_include_directories(${PROJECT_NAME} PRIVATE ${SPNG_ROOT}/spng)

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

FetchContent_Populate(
    php
    GIT_REPOSITORY https://github.com/h2o/picohttpparser.git
    GIT_TAG f8326098f63eefabfa2b6ec595d90e9ed5ed958a
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendor/php
)
target_include_directories(${PROJECT_NAME} PRIVATE ${php_SOURCE_DIR})
add_library(php_static STATIC ${php_SOURCE_DIR}/picohttpparser.c)
target_link_libraries(${PROJECT_NAME} PRIVATE php_static)

# Scripts for Generation
set(RAIO_HEADER "${CMAKE_SOURCE_DIR}/include/raio.h")
set(RAIO_CODECS_NAMES "${CMAKE_BINARY_DIR}/include/raio-gen/codec_names.h")
set(RAIO_ENUM_NAMES "${CMAKE_BINARY_DIR}/include/raio-gen/enum_names.h")
set(RAIO_WORKER_LIST "${CMAKE_BINARY_DIR}/include/raio-gen/worker_list.h")
file(GLOB RAIO_CODECS "${CMAKE_SOURCE_DIR}/src/Backend_Codecs/*.c")
string(REGEX REPLACE "${CMAKE_SOURCE_DIR}/src/Backend_Codecs/" "" RAIO_CODECS_STR "${RAIO_CODECS}")
string(JOIN "," RAIO_CODECS_STR ${RAIO_CODECS})
add_executable(gen_workers_list ${CMAKE_SOURCE_DIR}/scripts/gen_workers_list.c)
add_executable(gen_codec_names ${CMAKE_SOURCE_DIR}/scripts/gen_codec_names.c)
add_executable(gen_enum_names ${CMAKE_SOURCE_DIR}/scripts/gen_enum_names.c)
target_compile_definitions(gen_codec_names PRIVATE RAIO_CODECS=\"${RAIO_CODECS_STR}\")
target_compile_definitions(gen_enum_names PRIVATE RAIO_HEADER=\"${RAIO_HEADER}\")
target_include_directories(gen_workers_list PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(gen_workers_list PRIVATE ${CMAKE_BINARY_DIR}/include)
add_custom_command(
    OUTPUT ${RAIO_WORKER_LIST}
    COMMAND $<TARGET_FILE:gen_workers_list> > ${RAIO_WORKER_LIST}
    DEPENDS gen_workers_list;${RAIO_HEADER};${CMAKE_SOURCE_DIR}/include/raio/workers.h
)
add_custom_command(
    OUTPUT ${RAIO_CODECS_NAMES}
    COMMAND $<TARGET_FILE:gen_codec_names> > ${RAIO_CODECS_NAMES}
    DEPENDS gen_codec_names;${RAIO_CODECS};
)
add_custom_command(
    OUTPUT ${RAIO_ENUM_NAMES}
    COMMAND $<TARGET_FILE:gen_enum_names> > ${RAIO_ENUM_NAMES}
    DEPENDS gen_enum_names;${RAIO_HEADER};
)
add_custom_target(raio_worker_list DEPENDS ${RAIO_WORKER_LIST})
add_custom_target(raio_codecs_names DEPENDS ${RAIO_CODECS_NAMES})
add_custom_target(raio_enum_names DEPENDS ${RAIO_ENUM_NAMES})
add_dependencies(raio_worker_list raio_enum_names)
add_dependencies(${PROJECT_NAME} raio_worker_list raio_codecs_names)

# Scripts For Testing
option(RAIO_TEST "Unit Tests" OFF)
if(RAIO_TEST)
    include(CTest)
    set(tests
        "test_get_format_from_extension,src/Common_String/extensions.c"
    )
    foreach(deps IN LISTS tests)
        string(REPLACE "," ";${CMAKE_SOURCE_DIR}/" deps "${deps}")
        list(GET deps 0 tests_name)
        list(REMOVE_AT deps 0)
        add_executable("${tests_name}" "${CMAKE_SOURCE_DIR}/tests/${tests_name}.c;${deps}")
        target_include_directories(${tests_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
        add_test(NAME "${tests_name}" COMMAND "${tests_name}")
    endforeach()
endif()
