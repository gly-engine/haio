cmake_minimum_required(VERSION 3.19)
include(FetchContent)

project("haio" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
file(GLOB_RECURSE HAIO_SOURCES "${CMAKE_CURRENT_LIST_DIR}/source/*/*.cpp")
add_executable(${PROJECT_NAME} "${CMAKE_CURRENT_LIST_DIR}/source/main.cpp;${HAIO_SOURCES}")
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if (WINDOWS)
    file(GLOB_RECURSE HAIO_WINDOWS_SOURCES "${CMAKE_CURRENT_LIST_DIR}/source/platform/microslop/*.cpp")
    target_sources(${PROJECT_NAME} PRIVATE "${HAIO_WINDOWS_SOURCES}")
endif()
