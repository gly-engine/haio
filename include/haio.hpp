#include "haio_common.hpp"
#include "haio_iwindow.hpp"

namespace Haio {
    std::unique_ptr<IWindow> CreateWindow(const char* title, int width, int height);
}
