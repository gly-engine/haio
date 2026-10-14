#include <haio/implementation/platform/desktop/asio.hpp>

#include <boost/asio/post.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <csignal>
#include <iostream>
#include <memory>

namespace asio = boost::asio;

namespace Haio::Implementation::PlatformDesktop {

asio::io_context& io() {
    static asio::io_context context;
    return context;
}

}

namespace Haio::Implementation::Interfaces {

using PlatformDesktop::io;

void spawn(Platform::Task<void> task) {
    // posted rather than started here, so spawning from inside a task never runs the
    // new one ahead of the rest of the caller
    asio::post(io(), [task = std::move(task)]() mutable {
        detach(std::move(task), [](std::exception_ptr error) {
            if (!error) return;
            try {
                std::rethrow_exception(error);
            } catch (const std::exception& err) {
                std::cerr << "error: " << err.what() << "\n";
            } catch (...) {
                std::cerr << "error: a task failed with something that is not an exception\n";
            }
        });
    });
}

void run(Platform::Task<void> root) {
    /**
     * shared, because a stop() can end run() while root is still suspended, and root
     * finishing on some later run() must not write into this frame after it is gone.
     */
    struct Outcome {
        bool done = false;
        std::exception_ptr error;
    };
    auto outcome = std::make_shared<Outcome>();

    asio::post(io(), [root = std::move(root), outcome]() mutable {
        detach(std::move(root), [outcome](std::exception_ptr error) {
            outcome->done = true;
            outcome->error = error;
            io().stop();
        });
    });

    io().restart();
    io().run();

    if (outcome->error) std::rethrow_exception(outcome->error);
}

void stop() {
    io().stop();
}

void stopOnSignal() {
    // it holds the loop open, which is exactly right for a server and exactly wrong
    // for a command that should return when its work is done: only the cdn asks
    static asio::signal_set signals(io(), SIGINT, SIGTERM);
    signals.async_wait([](auto, auto) { io().stop(); });
}

Platform::Task<void> sleep(std::chrono::milliseconds duration) {
    co_await PlatformDesktop::await([](std::chrono::milliseconds wait) -> asio::awaitable<void> {
        asio::steady_timer timer(co_await asio::this_coro::executor, wait);
        co_await timer.async_wait(asio::use_awaitable);
    }(duration));
}

}
