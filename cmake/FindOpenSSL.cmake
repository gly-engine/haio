# Boost.Redis calls find_package(OpenSSL) and disables itself outright when the answer
# is no, so the answer has to be yes. It is the only thing it wants openssl for: the
# redis code compiles against <openssl/*.h>, which this build aims at wolfssl's shims,
# and the library it would have linked was never taken from -- --as-needed dropped it
# and ldd showed nothing.
#
# This module sits earlier on CMAKE_MODULE_PATH than the one that searches the machine,
# the same way FindZLIB does, and answers out of this build instead. What it buys is a
# builder that installs no ssl headers to throw away: before this, debian's libssl-dev
# was a build dependency of a binary that does not contain a byte of openssl.
#
# It is deliberately not a real search, and the targets are deliberately empty. wolfssl
# is added to this build after boost is, so there is no library to point at here yet;
# what links the SSL_* symbols is wolfssl on haio_core's own link line, which is where
# the choice of a tls library belongs.

foreach(component SSL Crypto)
    if(NOT TARGET OpenSSL::${component})
        add_library(OpenSSL::${component} INTERFACE IMPORTED)
    endif()
endforeach()

set(OPENSSL_FOUND TRUE)
set(OpenSSL_FOUND TRUE)
set(OPENSSL_INCLUDE_DIR "")
set(OPENSSL_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
set(OPENSSL_SSL_LIBRARY OpenSSL::SSL)
set(OPENSSL_CRYPTO_LIBRARY OpenSSL::Crypto)

# no OPENSSL_VERSION on purpose. nothing here asks for one, and a caller that does is
# asking after a real openssl's behaviour -- better it fails on the spot than believes
# a number made up to keep it quiet.
