#include "haio.hpp"
#include "window.hpp"

namespace Haio {

std::unique_ptr<IWindow> CreateWindow(const char* title, int width, int height) {
    return std::make_unique<WindowsWindow>(title, width, height);
}

}