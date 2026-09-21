# FetchContent keeps its "already downloaded" stamp inside the build tree while the
# sources live in vendor/, which is outside it. Deleting vendor/ therefore leaves a
# configured build convinced it has something it does not, and the failure surfaces
# much later as "does not contain a CMakeLists.txt". Checking for a file that has to
# exist makes the two agree again.

function(haio_fetch name url dir sentinel)
    if(NOT EXISTS "${dir}/${sentinel}")
        # the stamp lives in one of these two depending on the cmake version
        file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/${name}-subbuild")
        file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/_deps/${name}-subbuild")
        file(REMOVE_RECURSE "${dir}")
    endif()
    FetchContent_Populate(${name} URL "${url}" SOURCE_DIR "${dir}")
endfunction()
