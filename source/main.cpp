#include "haio.hpp"

auto main() -> int {
    auto window = Haio::CreateWindow("janela", 800, 600);

    window->init();

    while (!window->shouldClose()) {
        window->pollEvents();
    }

    window->shutdown();
    return 0;
}
