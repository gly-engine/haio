
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
# found, so cmake/FindOpenSSL.cmake answers that it is -- with two empty targets -- and
# they are dropped here rather than handed to everything downstream.
if(TARGET boost_redis)
    # an unset property reads back as HAIO_REDIS_LINKS-NOTFOUND, and writing that back
    # asks the linker for -lHAIO_REDIS_LINKS-NOTFOUND, which is how a Boost.Redis that
    # had quietly disabled itself used to report it: at link time, under a name no
    # library ever had.
    get_target_property(HAIO_REDIS_LINKS boost_redis INTERFACE_LINK_LIBRARIES)
    if(HAIO_REDIS_LINKS)
        list(REMOVE_ITEM HAIO_REDIS_LINKS OpenSSL::Crypto OpenSSL::SSL)
        set_target_properties(boost_redis PROPERTIES INTERFACE_LINK_LIBRARIES "${HAIO_REDIS_LINKS}")
    else()
        message(FATAL_ERROR
            "haio: Boost.Redis disabled itself, so the cdn would build without a redis cache. "
            "cmake/FindOpenSSL.cmake is what it looks for and it did not win the search.")
    endif()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::url Boost::beast Boost::redis)