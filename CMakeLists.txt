cmake_minimum_required(VERSION 3.19)
include(FetchContent)
include(CheckCXXCompilerFlag)

project("haio" LANGUAGES C CXX)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(PNG REQUIRED)
find_package(Boost REQUIRED COMPONENTS url)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
file(GLOB_RECURSE HAIO_LIB_SOURCES "${CMAKE_CURRENT_LIST_DIR}/library/*/*.cpp")

set(ETCPAK_VERSION "2.1")
set(ETCPAK_DIR "${CMAKE_SOURCE_DIR}/vendor/etcpak")
set(ETCPAK_DOWNLOAD "https://github.com/wolfpld/etcpak/archive/refs/tags/${ETCPAK_VERSION}.tar.gz")
FetchContent_Populate(etcpak URL "${ETCPAK_DOWNLOAD}" SOURCE_DIR "${ETCPAK_DIR}")
set(ETCPAK_SOURCES
    "${ETCPAK_DIR}/bcdec.c"
    "${ETCPAK_DIR}/Decode.cpp"
    "${ETCPAK_DIR}/Dither.cpp"
    "${ETCPAK_DIR}/ProcessRGB.cpp"
    "${ETCPAK_DIR}/Tables.cpp"
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64|i[3-6]86)$")
    check_cxx_compiler_flag("-msse4.1" HAIO_HAS_SSE41)
    if(HAIO_HAS_SSE41)
        set_source_files_properties(${ETCPAK_SOURCES} PROPERTIES COMPILE_OPTIONS "-msse4.1")
    endif()
endif()

set(WUFFS_VERSION "v0.3.4")
set(WUFFS_DIR "${CMAKE_SOURCE_DIR}/vendor/wuffs")
set(WUFFS_DOWNLOAD "https://github.com/google/wuffs-mirror-release-c/archive/refs/tags/${WUFFS_VERSION}.tar.gz")
FetchContent_Populate(wuffs URL "${WUFFS_DOWNLOAD}" SOURCE_DIR "${WUFFS_DIR}")

add_library(haio_core STATIC ${HAIO_LIB_SOURCES} ${ETCPAK_SOURCES})
target_include_directories(haio_core
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
    PRIVATE "${ETCPAK_DIR}" "${WUFFS_DIR}/release/c"
)
target_link_libraries(haio_core PUBLIC PNG::PNG PkgConfig::WEBP Boost::headers Boost::url Threads::Threads)

if(WIN32)
    file(GLOB_RECURSE HAIO_WINDOWS_SOURCES "${CMAKE_CURRENT_LIST_DIR}/source/platform/microslop/*.cpp")
    target_sources(haio_core PRIVATE ${HAIO_WINDOWS_SOURCES})
endif()

add_executable(${PROJECT_NAME} "${CMAKE_CURRENT_LIST_DIR}/source/main.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE haio_core)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(HAIO_TESTS "Unit Tests" OFF)
if(HAIO_TESTS)
    include(CTest)
    set(tests
        "test_image_convert_png_to_ppm"
        "test_image_etc1_roundtrip"
        "test_image_convert_png_to_etc1_ppm"
        "test_gpu_formats_roundtrip"
        "test_url_parsing"
    )
    foreach(test_name IN LISTS tests)
        add_executable("${test_name}" "${CMAKE_SOURCE_DIR}/tests/${test_name}.cpp")
        target_link_libraries("${test_name}" PRIVATE haio_core)
        if(test_name STREQUAL "test_image_convert_png_to_ppm")
            add_test(NAME "${test_name}" COMMAND "${test_name}" "${CMAKE_SOURCE_DIR}/assets/logo8x8.png" "${CMAKE_BINARY_DIR}/logo8x8_direct.ppm")
        elseif(test_name STREQUAL "test_image_convert_png_to_etc1_ppm")
            add_test(NAME "${test_name}" COMMAND "${test_name}" "${CMAKE_SOURCE_DIR}/assets/logo8x8.png" "${CMAKE_BINARY_DIR}/logo8x8_etc1.ppm")
        else()
            add_test(NAME "${test_name}" COMMAND "${test_name}")
        endif()
    endforeach()
endif()
