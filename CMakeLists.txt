cmake_minimum_required(VERSION 3.19)
include(FetchContent)
include(ExternalProject)

project("haio" LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
file(GLOB_RECURSE HAIO_LIB_SOURCES "${CMAKE_CURRENT_LIST_DIR}/library/*/*.cpp")
file(GLOB_RECURSE HAIO_BIN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/source/*/*.cpp")
add_executable(${PROJECT_NAME} "${CMAKE_CURRENT_LIST_DIR}/source/main.cpp;${HAIO_LIB_SOURCES};${HAIO_BIN_SOURCES}")
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if (WINDOWS)
    file(GLOB_RECURSE HAIO_WINDOWS_SOURCES "${CMAKE_CURRENT_LIST_DIR}/source/platform/microslop/*.cpp")
    target_sources(${PROJECT_NAME} PRIVATE "${HAIO_WINDOWS_SOURCES}")
endif()

option(HAIO_TESTS "Unit Tests" OFF)
if(HAIO_TESTS)
    include(CTest)
    set(tests
        "test_image_convert_png_to_ppm,library/codecs/png.cpp"
    )
    foreach(deps IN LISTS tests)
        string(REPLACE "," ";${CMAKE_SOURCE_DIR}/" deps "${deps}")
        list(GET deps 0 tests_name)
        list(REMOVE_AT deps 0)
        add_executable("${tests_name}" "${CMAKE_SOURCE_DIR}/tests/${tests_name}.cpp;${deps};${HAIO_LIB_SOURCES}")
        target_include_directories(${tests_name} PRIVATE "${CMAKE_SOURCE_DIR}/include;${CMAKE_BINARY_DIR}/include")
        add_test(NAME "${tests_name}" COMMAND "${tests_name}")
    endforeach()
endif()

set(WUFFS_VERSION "v0.3.4")
set(WUFFS_DIR "${CMAKE_SOURCE_DIR}/vendor/wuffs")
set(WUFFS_DOWNLOAD "https://github.com/google/wuffs-mirror-release-c/archive/refs/tags/${WUFFS_VERSION}.tar.gz")
FetchContent_Populate(wuffs URL "${WUFFS_DOWNLOAD}" SOURCE_DIR "${WUFFS_DIR}")
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/library/codecs/png.cpp" PROPERTIES INCLUDE_DIRECTORIES "${WUFFS_DIR}/release/c")

set(VULKAN_VERSION "v1.3.290")
set(VULKAN_DIR "${CMAKE_SOURCE_DIR}/vendor/vulkan/vulkan")
set(VULKAN_DOWNLOAD "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/${VULKAN_VERSION}.tar.gz")
FetchContent_Populate(vulkan URL ${VULKAN_DOWNLOAD} SOURCE_DIR ${VULKAN_DIR})

set(VOLK_VERSION "1.4.350")
set(VOLK_DIR "${CMAKE_SOURCE_DIR}/vendor/vulkan/volk")
set(VOLK_DOWNLOAD "https://github.com/zeux/volk/archive/refs/tags/${VOLK_VERSION}.tar.gz")
FetchContent_Populate(volk URL "${VOLK_DOWNLOAD}" SOURCE_DIR "${VOLK_DIR}")
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/library/vulkan/runtime.cpp" PROPERTIES INCLUDE_DIRECTORIES "${VOLK_DIR};${VULKAN_DIR}/include")
add_library(volk STATIC ${VOLK_DIR}/volk.c)
target_link_libraries(${PROJECT_NAME} PRIVATE volk)
target_include_directories(volk SYSTEM PRIVATE "${VULKAN_DIR}/include")

if(FALSE)
set(SHADERC_VERSION "v2026.2")
set(SHADERC_DIR "${CMAKE_SOURCE_DIR}/vendor/vulkan/shaderc")
set(SHADERC_DOWNLOAD "https://github.com/google/shaderc/archive/refs/tags/${SHADERC_VERSION}.tar.gz")
ExternalProject_Add(shaderc
    URL ${SHADERC_DOWNLOAD}
    PREFIX ${CMAKE_BINARY_DIR}/shaderc
    SOURCE_DIR ${SHADERC_DIR}
    UPDATE_COMMAND python utils/git-sync-deps
    PATCH_COMMAND sed -i "s/if *(GLSLANG_ENABLE_INSTALL)/if(FALSE)/" ${SHADERC_DIR}/third_party/glslang/CMakeLists.txt
    CMAKE_ARGS
        -DSHADERC_SKIP_TESTS=ON
        -DSHADERC_SKIP_INSTALL=ON
        -DCMAKE_BUILD_TYPE=Release
    BUILD_COMMAND cmake --build <BINARY_DIR> --target glslc
    INSTALL_COMMAND ""
)

add_executable(xxdbin scripts/xxd.cpp)
file(GLOB_RECURSE GLSL_SOURCES "${CMAKE_SOURCE_DIR}/shaders/*.glsl")
set(GENERATED_HEADERS "")
foreach(GLSL ${GLSL_SOURCES})
    get_filename_component(GLSL_NAME ${GLSL} NAME_WE)

    set(SPIRV_OUT ${CMAKE_BINARY_DIR}/shaders/${GLSL_NAME}.spirv)
    set(HEADER_OUT ${CMAKE_BINARY_DIR}/include/shaders/${GLSL_NAME}.h)

    add_custom_command(
        OUTPUT ${SPIRV_OUT}
        COMMAND glslc ${GLSL} -o ${SPIRV_OUT}
        DEPENDS ${GLSL}
        COMMENT "Compiling ${GLSL} -> ${SPIRV_OUT}"
    )

    add_custom_command(
        OUTPUT ${HEADER_OUT}
        COMMAND xxdbin ${SPIRV_OUT} ${HEADER_OUT}
        DEPENDS ${SPIRV_OUT} xxdbin
        COMMENT "Embedding ${SPIRV_OUT} -> ${HEADER_OUT}"
    )

    list(APPEND GENERATED_HEADERS ${HEADER_OUT})
endforeach()
add_custom_target(shader_headers DEPENDS ${GENERATED_HEADERS})
add_dependencies(${PROJECT_NAME} shader_headers)
endif()
