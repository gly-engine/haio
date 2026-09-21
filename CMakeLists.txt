cmake_minimum_required(VERSION 3.19)
include(FetchContent)
include(CheckCXXCompilerFlag)

project("haio" LANGUAGES C CXX)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
# ours comes first, so FindZLIB here wins over the one that searches the system
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include("${CMAKE_CURRENT_LIST_DIR}/cmake/fetch.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/cpp26.cmake")

# everything the binary needs is built into it.
#
# wolfssl declares BUILD_SHARED_LIBS as an option defaulting to ON, and an option only
# takes effect when nobody has set the cache entry already. Left alone it therefore
# turned boost, wolfssl and zlib-ng into .so files the binary had to find at run time,
# which is nobody's decision, just whoever was asked first.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# and neither does the binary: not libstdc++, not libgcc, not libc.
#
# running on whatever is in front of it is the point of a converter -- an old distro, a
# scratch container, somebody else's build box -- and the four shared libraries it
# still wanted were the only thing that made that a question. the toolchain is asked
# here rather than at link time, because a missing libc.a fails as a screenful of
# undefined references with nothing in it that says which package to install.
#
# hostname resolution is the exception, and it is glibc's rather than haio's:
# getaddrinfo loads its nss plugins at run time whatever the binary was linked as, so
# the cdn wants a host whose glibc matches the one it was built against. converting a
# file resolves nothing and does not care.
option(HAIO_STATIC "link the binary against nothing at all" ON)

if(HAIO_STATIC)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LINK_OPTIONS "-static")
    check_cxx_source_compiles("int main() { return 0; }" HAIO_HAS_STATIC_LINK)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)

    if(NOT HAIO_HAS_STATIC_LINK)
        message(FATAL_ERROR
            "haio links its binary static and this toolchain has no static c++ runtime to link "
            "against. debian and ubuntu keep it in libc6-dev and libstdc++-<version>-dev; "
            "configure with -DHAIO_STATIC=OFF to link the shared ones instead.")
    endif()

    # gcc hands libgcc and libatomic to the linker as "gcc_s_asneeded" and
    # "atomic_asneeded", names of no library at all: its own specs rewrite each into an
    # --as-needed pair on the way past. cmake reads the list out of a shared link once,
    # at configure time, and repeats it on every link line afterwards, so with -static
    # the driver stops rewriting them and ld is handed two libraries that were never
    # files. dropping them lets the driver put the static libgcc there itself, which is
    # what the static branch of that same spec already says to do.
    foreach(language C CXX)
        set(HAIO_IMPLICIT_LINK "")
        foreach(library IN LISTS CMAKE_${language}_IMPLICIT_LINK_LIBRARIES)
            if(NOT library MATCHES "_asneeded$")
                list(APPEND HAIO_IMPLICIT_LINK "${library}")
            endif()
        endforeach()
        set(CMAKE_${language}_IMPLICIT_LINK_LIBRARIES "${HAIO_IMPLICIT_LINK}")
    endforeach()
endif()

find_package(Threads REQUIRED)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
file(GLOB_RECURSE HAIO_LIB_SOURCES "${CMAKE_CURRENT_LIST_DIR}/library/*.cpp")

set(BOOST_VERSION "1.91.0-1")
set(BOOST_DIR "${CMAKE_SOURCE_DIR}/vendor/boost")
set(BOOST_DOWNLOAD "https://github.com/boostorg/boost/releases/download/boost-${BOOST_VERSION}/boost-${BOOST_VERSION}-cmake.tar.xz")
set(BOOST_INCLUDE_LIBRARIES url asio beast redis spirit CACHE STRING "" FORCE)
haio_fetch(boost "${BOOST_DOWNLOAD}" "${BOOST_DIR}" CMakeLists.txt)
add_subdirectory("${BOOST_DIR}" "${CMAKE_BINARY_DIR}/boost" EXCLUDE_FROM_ALL)

# Boost.Redis asks for the system openssl and hands it to everything downstream, which
# is one ssl library more than a binary should have: haio's is wolfssl, with the
# openssl headers aimed at its shims, so the redis code was already calling wolfssl's
# symbols and linking somebody else's library to find them. nothing was ever taken
# from it -- --as-needed dropped it and ldd showed nothing -- so this surfaced only
# once the link went static and ld refused to read a shared object at all.
#
# it cannot simply be hidden: Boost.Redis disables itself outright when openssl is not
# found, so it is found, and then removed from what it passes on.
if(TARGET boost_redis)
    get_target_property(HAIO_REDIS_LINKS boost_redis INTERFACE_LINK_LIBRARIES)
    list(REMOVE_ITEM HAIO_REDIS_LINKS OpenSSL::Crypto OpenSSL::SSL)
    set_target_properties(boost_redis PROPERTIES INTERFACE_LINK_LIBRARIES "${HAIO_REDIS_LINKS}")
endif()

set(WOLFSSL_VERSION "v5.9.2-stable")
set(WOLFSSL_DIR "${CMAKE_SOURCE_DIR}/vendor/wolfssl")
set(WOLFSSL_DOWNLOAD "https://github.com/wolfSSL/wolfssl/archive/refs/tags/${WOLFSSL_VERSION}.tar.gz")
set(WOLFSSL_ASIO yes CACHE STRING "" FORCE)
set(WOLFSSL_EXAMPLES no CACHE STRING "" FORCE)
set(WOLFSSL_CRYPT_TESTS no CACHE STRING "" FORCE)
set(WOLFSSL_INSTALL no CACHE STRING "" FORCE)
haio_fetch(wolfssl "${WOLFSSL_DOWNLOAD}" "${WOLFSSL_DIR}" CMakeLists.txt)
add_subdirectory("${WOLFSSL_DIR}" "${CMAKE_BINARY_DIR}/wolfssl" EXCLUDE_FROM_ALL)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/zlib-ng.cmake")

# wolfssl keeps WOLFSSL_ASIO's defines PRIVATE and exports only its own root, so a
# consumer has to repeat them and aim <openssl/*.h> at the shims, or the system
# openssl headers win and the link fails on plain SSL_* symbols
set(HAIO_TLS_DEFINITIONS
    BOOST_ASIO_USE_WOLFSSL OPENSSL_ALL OPENSSL_EXTRA HAVE_EX_DATA
    WOLFSSL_KEY_GEN HAVE_OCSP OPENSSL_NO_SSL2 OPENSSL_NO_SSL3)

add_executable(xxd "${CMAKE_CURRENT_LIST_DIR}/scripts/xxd.cpp")
add_executable(replace "${CMAKE_CURRENT_LIST_DIR}/scripts/replace.cpp")
# the grammar builds its format and colour names out of the enums, so the doc
# generator needs reflection as much as the library does
add_executable(convert_ebnf "${CMAKE_CURRENT_LIST_DIR}/scripts/convert_ebnf.cpp")
target_include_directories(convert_ebnf PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_options(convert_ebnf PRIVATE ${HAIO_CXX_FLAGS})
target_link_libraries(convert_ebnf PRIVATE Boost::spirit)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_LIST_DIR}/docs/convert-ebnf.md"
    COMMAND $<TARGET_FILE:convert_ebnf> > "${CMAKE_CURRENT_LIST_DIR}/docs/convert-ebnf.md"
    DEPENDS convert_ebnf
)
add_custom_target(haio_convert_docs ALL DEPENDS "${CMAKE_CURRENT_LIST_DIR}/docs/convert-ebnf.md")

# the codec registry is the source tree itself: every "template <> ... X<Format::F, Color::P>"
# found in a codec becomes a declaration, which is all the concepts need to see.
#
# it runs at build time and not at configure time on purpose. CONFIGURE_DEPENDS
# re-globs when the set of files changes, never when their contents do, so editing
# a codec to add or drop a capability would otherwise leave a stale header behind.
file(GLOB HAIO_CONTAINER_DIRS CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/library/backend/codecs/container/*")
file(GLOB_RECURSE HAIO_COLOR_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/library/backend/codecs/color/*.cpp")
list(SORT HAIO_CONTAINER_DIRS)
list(SORT HAIO_COLOR_SOURCES)

set(HAIO_CODEC_SOURCES "")
set(HAIO_CODEC_SOURCES_EXTRA "")
set(HAIO_CODEC_LIBRARIES "")
set(HAIO_CODECS_OFF "")
set(HAIO_CODEC_DEPENDS "")

# one directory per container, one file per capability.
#
# there is no switch for a codec as such. a codec that needs nothing costs a
# translation unit and answers for a format people have files of, so turning it off
# buys nothing worth a flag; what is worth switching is a third party library, and
# those have their own: -DHAIO_USE_SPNG=OFF drops png encoding and keeps detect and
# decode, -DHAIO_USE_ETCPAK=OFF drops etc1 both ways. the codec's own cmake is where
# each of those lives, and dropping a source file is how it says so.
foreach(HAIO_CODEC_DIR IN LISTS HAIO_CONTAINER_DIRS)
    if(NOT IS_DIRECTORY "${HAIO_CODEC_DIR}")
        continue()
    endif()
    get_filename_component(name "${HAIO_CODEC_DIR}" NAME)

    file(GLOB HAIO_CODEC_SOURCES_THIS CONFIGURE_DEPENDS "${HAIO_CODEC_DIR}/*.cpp")
    list(SORT HAIO_CODEC_SOURCES_THIS)

    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/cmake/codec_${name}.cmake")
        include("${CMAKE_CURRENT_LIST_DIR}/cmake/codec_${name}.cmake")
    endif()

    list(APPEND HAIO_CODEC_SOURCES ${HAIO_CODEC_SOURCES_THIS})
    file(GLOB every "${HAIO_CODEC_DIR}/*.cpp")
    foreach(source IN LISTS every)
        if(NOT source IN_LIST HAIO_CODEC_SOURCES_THIS)
            list(REMOVE_ITEM HAIO_LIB_SOURCES "${source}")
            get_filename_component(capability "${source}" NAME_WE)
            list(APPEND HAIO_CODECS_OFF "${name}/${capability}")
        endif()
    endforeach()
endforeach()

# colour conversions come in pairs of files rather than directories, so their cmake
# attaches dependencies by name
foreach(colour IN ITEMS etc1)
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/cmake/color_${colour}.cmake")
        include("${CMAKE_CURRENT_LIST_DIR}/cmake/color_${colour}.cmake")
    endif()
endforeach()
list(APPEND HAIO_CODEC_SOURCES ${HAIO_COLOR_SOURCES})

if(HAIO_CODECS_OFF)
    message(STATUS "haio: disabled -- ${HAIO_CODECS_OFF}")
endif()

set(HAIO_CODECS_HEADER "${CMAKE_BINARY_DIR}/include/haio_codecs.hpp")
set(HAIO_FORMATS_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/haio_formats.hpp")
add_executable(gen_codecs "${CMAKE_CURRENT_LIST_DIR}/scripts/gen_codecs.cpp")
add_custom_command(
    OUTPUT "${HAIO_CODECS_HEADER}"
    COMMAND $<TARGET_FILE:gen_codecs> "${HAIO_CODECS_HEADER}"
            --formats "${HAIO_FORMATS_HEADER}" ${HAIO_CODEC_SOURCES}
    DEPENDS gen_codecs "${HAIO_FORMATS_HEADER}" ${HAIO_CODEC_SOURCES}
    COMMENT "scanning codecs for their capabilities"
)
add_custom_target(haio_codecs DEPENDS "${HAIO_CODECS_HEADER}")

add_library(haio_core STATIC ${HAIO_LIB_SOURCES} ${HAIO_CODEC_SOURCES_EXTRA})
# nothing third party leaks in here: each codec attaches what it needs to its own
# source file in cmake/codec_<name>.cmake
target_include_directories(haio_core
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
           "${CMAKE_BINARY_DIR}/include"
    # the cdn keeps its own headers beside its sources, since nothing outside it
    # has any use for a bucket fetcher
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/library/frontend/cdn"
)

# how haio_core is built, which is not the same list as what it is.
#
# a codec includes turbojpeg.h and the cdn includes beast, and both do it in their own
# .cpp files: nothing that links haio_core has any business seeing those headers. they
# were PUBLIC, though, so every test compiled with wolfssl, zlib and four boost
# libraries on its include path to build a picture in memory. LINK_ONLY is the way to
# say the rest of what PUBLIC meant -- a consumer still links the object code, because
# a static library carries none of its own.
set(HAIO_INTERNAL_LIBRARIES ${HAIO_CODEC_LIBRARIES} wolfssl zlib Boost::url Boost::beast Boost::redis)
set(HAIO_INTERNAL_LINK "")
foreach(library IN LISTS HAIO_INTERNAL_LIBRARIES)
    list(APPEND HAIO_INTERNAL_LINK "$<LINK_ONLY:${library}>")
endforeach()

# and what it is: spirit is in the command line grammar and asio is in the cdn's
# public headers, so those two come with the library whether a consumer wants them or not
target_link_libraries(haio_core
    PRIVATE ${HAIO_INTERNAL_LIBRARIES}
    INTERFACE ${HAIO_INTERNAL_LINK}
    PUBLIC Boost::asio Boost::spirit Threads::Threads)
target_compile_options(haio_core PUBLIC ${HAIO_CXX_FLAGS})
add_dependencies(haio_core haio_codecs)
if(HAIO_CODEC_DEPENDS)
    add_dependencies(haio_core ${HAIO_CODEC_DEPENDS})
endif()
target_compile_definitions(haio_core PRIVATE ${HAIO_TLS_DEFINITIONS})
target_include_directories(haio_core BEFORE PRIVATE "${WOLFSSL_DIR}/wolfssl")

if(WIN32)
    file(GLOB_RECURSE HAIO_WINDOWS_SOURCES "${CMAKE_CURRENT_LIST_DIR}/source/platform/microslop/*.cpp")
    target_sources(haio_core PRIVATE ${HAIO_WINDOWS_SOURCES})
endif()

add_executable(${PROJECT_NAME} "${CMAKE_CURRENT_LIST_DIR}/source/main.cpp")
add_dependencies(${PROJECT_NAME} haio_convert_docs)
target_link_libraries(${PROJECT_NAME} PRIVATE haio_core)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# only the binary. a test is run out of the build directory it was built in, where the
# shared libraries it wants are the ones it was built against, and linking every one of
# them static is a copy of libstdc++ per test for nothing.
if(HAIO_STATIC)
    target_link_options(${PROJECT_NAME} PRIVATE -static)
endif()

option(HAIO_TESTS "Unit Tests" OFF)
if(HAIO_TESTS)
    include(CTest)
    set(tests
        "test_image_convert_png_to_ppm"
        "test_image_etc1_roundtrip"
        "test_image_convert_png_to_etc1_ppm"
        "test_gpu_formats_roundtrip"
        "test_url_parsing"
        "test_convert_parser"
        "test_format_from_magic"
        "test_encode_zcis"
        "test_color_rgba8888_to_rgb888"
        "test_decode_netpbm"
        "test_format_mime"
        "test_transform_concepts"
        "test_cdn_cache"
        "test_cdn_zip"
        "test_cdn_s3"
        "test_palette"
        "test_png_channels"
        "test_codec_tga"
        "test_color_packed"
    )
    foreach(test_name IN LISTS tests)
        # a test named after something that is switched off has nothing left to
        # assert. what is switched off is written "png/encode" or "color/etc1", and a
        # test is named after the thing rather than the capability, so the thing is
        # what it is matched against: turning off one half of png skips the png tests
        # whole, which is the safe way round to be wrong.
        set(skip_test OFF)
        foreach(disabled IN LISTS HAIO_CODECS_OFF)
            string(REPLACE "/" ";" pieces "${disabled}")
            list(GET pieces 0 thing)
            if(thing STREQUAL "color")
                list(GET pieces 1 thing)
            endif()
            if(test_name MATCHES "${thing}")
                set(skip_test ON)
            endif()
        endforeach()
        if(skip_test)
            message(STATUS "haio: skipping ${test_name}, its codec is disabled")
            continue()
        endif()

        add_executable("${test_name}" "${CMAKE_SOURCE_DIR}/tests/${test_name}.cpp")
        target_link_libraries("${test_name}" PRIVATE haio_core)
        # a release build defines NDEBUG, and half these tests say what they mean with
        # assert. without this they compile to a main that returns zero and a suite
        # that passes by having checked nothing
        target_compile_options("${test_name}" PRIVATE -UNDEBUG)
        # the cdn keeps its headers beside its sources, and a couple of tests reach in
        target_include_directories("${test_name}" PRIVATE "${CMAKE_CURRENT_LIST_DIR}/library/frontend/cdn")
        if(test_name STREQUAL "test_image_convert_png_to_ppm")
            add_test(NAME "${test_name}" COMMAND "${test_name}" "${CMAKE_SOURCE_DIR}/assets/logo8x8.png" "${CMAKE_BINARY_DIR}/logo8x8_direct.ppm")
        elseif(test_name STREQUAL "test_image_convert_png_to_etc1_ppm")
            add_test(NAME "${test_name}" COMMAND "${test_name}" "${CMAKE_SOURCE_DIR}/assets/logo8x8.png" "${CMAKE_BINARY_DIR}/logo8x8_etc1.ppm")
        else()
            add_test(NAME "${test_name}" COMMAND "${test_name}")
        endif()
    endforeach()
endif()
