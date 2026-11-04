#if defined(_WIN32)

#include <haio_platform.hpp>

namespace Haio::Platform {

std::string homeFolder() {
    return {};
}

/** ntfs keeps the case a name was written in and finds it whatever case it is asked in */
bool namesKeepCase() {
    return false;
}

}

#endif
