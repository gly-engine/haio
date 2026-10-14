#pragma once

#include "haio/implementation/platform/task.hpp"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <utility>

/**
 * everything haio needs from the machine it runs on, and nothing it does with it.
 *
 * one directory under library/implementation/platform answers all of this, picked by HAIO_PLATFORM
 * at configure time. desktop does it with asio, beast and a socket; a browser build
 * would do the client half with fetch() and leave the server half out, since a page
 * does not listen on a port. nothing outside library/implementation/platform includes asio.
 *
 * the split follows who opens the connection:
 *
 *   runtime   the loop everything runs on
 *   client    haio asking somebody else for bytes: http, and redis on the desktop
 *   server    somebody asking haio: the cdn's listener
 */
namespace Haio::Implementation::Interfaces {

/** @name runtime */
///@{

/** starts a task on the loop with nobody waiting for it; what it throws is logged */
void spawn(Platform::Task<void> task);

/**
 * drives the loop until root is done or stop() is called, and rethrows whatever root
 * threw. the loop is the platform's own, so what spawn() started earlier runs too.
 */
void run(Platform::Task<void> root);

/** makes run() return, from inside the loop or from a signal */
void stop();

/** SIGINT and SIGTERM stop the loop rather than the process, so run() returns */
void stopOnSignal();

/** resumes the caller once the time has passed, without holding up the loop */
Platform::Task<void> sleep(std::chrono::milliseconds duration);

/**
 * a task's answer, for code that has nothing better to do than wait for it: the
 * convert command line, a test. it drives the loop, so it is never called from
 * inside a task.
 */
template <typename T>
T blockOn(Platform::Task<T> task) {
    std::optional<T> out;
    run([](Platform::Task<T> inner, std::optional<T>& into) -> Platform::Task<void> {
        into.emplace(co_await std::move(inner));
    }(std::move(task), out));
    if (!out) throw std::runtime_error("the loop was stopped before the task finished");
    return *std::move(out);
}

///@}

}
