#if !defined(_WIN32)

#include <haio_platform.hpp>

#include <cstdlib>

namespace Haio::Platform {

std::string homeFolder() {
    const char* home = std::getenv("HOME");
    return home ? home : "";
}

bool namesKeepCase() {
    return true;
}

}

#endif
