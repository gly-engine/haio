#pragma once
#include "haio_iwindow.hpp"

namespace Haio {

class IWindow {
public:
    virtual ~IWindow() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;
    virtual void pollEvents() = 0;
    virtual bool shouldClose() const = 0;
};

}