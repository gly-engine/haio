#pragma once

#include <haio_platform.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>

#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>

/**
 * the seam between asio and everything above it, and the only header that knows
 * both. nothing outside library/platform/desktop includes it.
 *
 * the code in here is written the way asio likes, as asio::awaitable, and crosses
 * into a Task at the edge with await(). the other direction is never needed: a Task
 * that has to wait for asio awaits, and asio never waits for a Task.
 */
namespace Haio::Platform::Desktop {

/** the one loop, on the one thread that runs it */
boost::asio::io_context& io();

template <typename T>
class AsioAwaiter {
public:
    explicit AsioAwaiter(boost::asio::awaitable<T> op) : op_(std::move(op)) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> waiting) {
        // co_spawn wants a default for the value when it hands over an exception, and
        // a socket has none, so the value travels in an optional that always has one
        boost::asio::co_spawn(io(), wrap(std::move(op_)),
                              [this, waiting](std::exception_ptr error, std::optional<Value> value) {
                                  error_ = error;
                                  value_ = std::move(value);
                                  waiting.resume();
                              });
    }

    T await_resume() {
        if (error_) std::rethrow_exception(error_);
        if constexpr (!std::is_void_v<T>) return std::move(**value_);
    }

private:
    struct Nothing {};
    using Value = std::conditional_t<std::is_void_v<T>, Nothing, T>;

    static boost::asio::awaitable<std::optional<Value>> wrap(boost::asio::awaitable<T> op) {
        if constexpr (std::is_void_v<T>) {
            co_await std::move(op);
            co_return Nothing{};
        } else {
            co_return co_await std::move(op);
        }
    }

    boost::asio::awaitable<T> op_;
    std::exception_ptr error_;
    std::optional<std::optional<Value>> value_;
};

/** co_await await(someAsioCoroutine(...)) from inside a Task */
template <typename T>
AsioAwaiter<T> await(boost::asio::awaitable<T> op) {
    return AsioAwaiter<T>(std::move(op));
}

}
