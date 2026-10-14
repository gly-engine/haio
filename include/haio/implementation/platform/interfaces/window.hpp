#pragma once

#include <memory>

/** a window on the screen, for the platforms that have one */
namespace Haio::Implementation::Interfaces {

class IWindow {
public:
    virtual ~IWindow() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;
    virtual void pollEvents() = 0;
    virtual bool shouldClose() const = 0;
};

std::unique_ptr<IWindow> CreateWindow(const char* title, int width, int height);

}
