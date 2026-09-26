# The codec registry asks the enum what it holds instead of carrying a list, and the
# capability concepts need reflection to answer. Check it here so the build stops on a
# readable message rather than a thousand template errors deep.

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckCXXSourceCompiles)

set(CMAKE_REQUIRED_FLAGS "-std=c++26 -freflection")
check_cxx_source_compiles("
#include <meta>
enum class E { A, B };
int main() {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        static_assert(std::meta::identifier_of(e).size() == 1);
    }
}
" HAIO_HAS_REFLECTION)
unset(CMAKE_REQUIRED_FLAGS)

if(NOT HAIO_HAS_REFLECTION)
    message(FATAL_ERROR "haio needs a compiler with working c++26 reflection (gcc 16+ with -freflection)")
endif()

target_compile_options(${PROJECT_NAME} PRIVATE -std=c++26 -freflection)