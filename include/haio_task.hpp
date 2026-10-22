#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>
#include <vector>

/**
 * the one way haio waits for something, whatever is doing the waiting underneath.
 *
 * it used to be boost::asio::awaitable, which is fine on a desktop and meaningless in
 * a browser: every signature that spelled it tied the code behind it to asio, the cdn
 * cache and the buckets included. a Task knows nothing about who resumes it, so the
 * platform decides that -- an io_context on the desktop, a fetch callback in wasm --
 * and everything above the platform is written once.
 *
 * it is lazy: nothing runs until somebody co_awaits it, and a Task nobody awaits
 * never starts. the awaiting coroutine is resumed straight from the final suspend, so
 * a chain of them costs no stack and no scheduler.
 */
namespace Haio {

template <typename T = void>
class Task;

namespace Detail {

struct PromiseBase {
    std::coroutine_handle<> continuation = std::noop_coroutine();
    std::exception_ptr error;

    std::suspend_always initial_suspend() noexcept { return {}; }

    struct Final {
        bool await_ready() noexcept { return false; }
        template <typename Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> self) noexcept {
            return self.promise().continuation;
        }
        void await_resume() noexcept {}
    };
    Final final_suspend() noexcept { return {}; }

    void unhandled_exception() noexcept { error = std::current_exception(); }

    void rethrow() const {
        if (error) std::rethrow_exception(error);
    }
};

template <typename T>
struct Promise : PromiseBase {
    std::optional<T> value;

    Task<T> get_return_object() noexcept;

    template <typename U = T>
    void return_value(U&& made) {
        value.emplace(std::forward<U>(made));
    }

    T take() {
        rethrow();
        return std::move(*value);
    }
};

template <>
struct Promise<void> : PromiseBase {
    Task<void> get_return_object() noexcept;
    void return_void() noexcept {}
    void take() const { rethrow(); }
};

}

template <typename T>
class [[nodiscard]] Task {
public:
    using promise_type = Detail::Promise<T>;
    using Handle = std::coroutine_handle<promise_type>;

    Task() = default;
    explicit Task(Handle handle) noexcept : handle_(handle) {}
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task() {
        if (handle_) handle_.destroy();
    }

    /** only an rvalue is awaited, so a Task is spent the moment somebody waits on it */
    auto operator co_await() && noexcept {
        struct Awaiter {
            Handle handle;
            bool await_ready() const noexcept { return !handle || handle.done(); }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> waiting) noexcept {
                handle.promise().continuation = waiting;
                return handle;
            }
            T await_resume() { return handle.promise().take(); }
        };
        return Awaiter{handle_};
    }

private:
    Handle handle_;
};

namespace Detail {

template <typename T>
Task<T> Promise<T>::get_return_object() noexcept {
    return Task<T>{std::coroutine_handle<Promise<T>>::from_promise(*this)};
}

inline Task<void> Promise<void>::get_return_object() noexcept {
    return Task<void>{std::coroutine_handle<Promise<void>>::from_promise(*this)};
}

/**
 * a coroutine that starts at once and frees itself when it ends, which is what lets
 * a Task run with nobody awaiting it. the platform uses it to start work on its loop;
 * nothing above the platform should need it.
 */
struct Detached {
    struct promise_type {
        Detached get_return_object() noexcept { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        // the body below catches everything, so reaching here is a bug in it
        void unhandled_exception() noexcept { std::terminate(); }
    };
};

}

/**
 * runs the task to the end and hands done() whatever it threw, or nothing.
 *
 * done is taken by value and lives in this frame, so a callback that captures state
 * owns it for exactly as long as the task runs.
 */
template <typename Done>
Detail::Detached detach(Task<void> task, Done done) {
    std::exception_ptr error;
    try {
        co_await std::move(task);
    } catch (...) {
        error = std::current_exception();
    }
    done(error);
}

/**
 * something coroutines wait for until somebody says it happened.
 *
 * the waiters are resumed right inside set(), one after the other, so this is only for
 * code that runs on one thread -- which is everything above the platform, since each
 * platform drives its loop from one.
 */
class Event {
public:
    bool isSet() const noexcept { return set_; }

    void set() {
        set_ = true;
        for (const auto waiting : std::exchange(waiters_, {})) waiting.resume();
    }

    auto operator co_await() noexcept {
        struct Awaiter {
            Event& event;
            bool await_ready() const noexcept { return event.set_; }
            void await_suspend(std::coroutine_handle<> waiting) { event.waiters_.push_back(waiting); }
            void await_resume() const noexcept {}
        };
        return Awaiter{*this};
    }

private:
    bool set_ = false;
    std::vector<std::coroutine_handle<>> waiters_;
};

}
