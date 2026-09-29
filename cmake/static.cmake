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

    target_link_options(${PROJECT_NAME} PRIVATE -static)

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