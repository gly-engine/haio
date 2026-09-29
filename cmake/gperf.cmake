option(HAIO_USE_PROFILER "use google gperftools" OFF)

set(GPERFTOOLS_VERSION "gperftools-2.18")
set(GPERFTOOLS_REPO "https://github.com/gperftools/gperftools.git")
set(GPERFTOOLS_DIR "${CMAKE_SOURCE_DIR}/vendor/gperftools")
set(GPERFTOOLS_BUILD_CPU_PROFILER ON CACHE BOOL "" FORCE)
set(GPERFTOOLS_BUILD_HEAP_PROFILER OFF CACHE BOOL "" FORCE)
set(GPERFTOOLS_BUILD_DEBUGALLOC OFF CACHE BOOL "" FORCE)
set(gperftools_build_minimal OFF CACHE BOOL "" FORCE)
set(gperftools_build_benchmark OFF CACHE BOOL "" FORCE)
set(gperftools_enable_libunwind ON CACHE BOOL "" FORCE)
set(gperftools_enable_stacktrace_via_backtrace ON CACHE BOOL "" FORCE)
set(gperftools_enable_frame_pointers ON CACHE BOOL "" FORCE)

if(HAIO_USE_PROFILER)
    FetchContent_Declare(profiler GIT_REPOSITORY ${GPERFTOOLS_REPO} GIT_TAG ${GPERFTOOLS_VERSION} SOURCE_DIR ${GPERFTOOLS_DIR})
    FetchContent_MakeAvailable(profiler)
    target_link_libraries(${PROJECT_NAME} PRIVATE profiler)
    target_compile_options(haio PRIVATE -g -fno-omit-frame-pointer)

    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/source/main.cpp"
        PROPERTIES
            INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/vendor/gperftools/src"
            COMPILE_DEFINITIONS "HAIO_USE_PROFILER"
    )
endif()
