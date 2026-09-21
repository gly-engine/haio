#pragma once

#include "haio_iwindow.hpp"

namespace Haio {

class WindowsWindow : public IWindow {
public:
    WindowsWindow(const char* title, int width, int height);
    ~WindowsWindow() override;

    void init() override;
    void shutdown() override;
    void pollEvents() override;
    bool shouldClose() const override;

private:
    const char *title;
    int width;
    int height;

    struct Impl;
    std::unique_ptr<Impl> p;
};

}