#pragma once

#include <expected>
#include <string>

/**
 * what goes wrong, as a value: every stage answers with a Result, and a failure says
 * why without anybody between it and the caller having to catch it.
 */
namespace Haio::Implementation {

enum class ErrorCode {
    Internal,
    InvalidInput,
    UnsupportedFormat,
    NotFound,
    Io,
    Upstream,
    Timeout,
};

struct Error {
    ErrorCode code = ErrorCode::Internal;
    std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

}

/**
 * go style early return. c++26 still has no "?" operator, so unwrapping a Result
 * and leaving on failure is either this or a monadic chain that reads badly when
 * the steps are independent.
 */
#define HAIO_TRY(name, expr)                                          \
    auto name##Result = (expr);                                       \
    if (!name##Result) return std::unexpected(name##Result.error());  \
    auto& name = *name##Result

/** for a check that only reports a problem and produces no value */
#define HAIO_CHECK(expr)                                              \
    do {                                                              \
        if (auto problem = (expr)) return std::unexpected(*problem);  \
    } while (false)

/** a plain failure, for the common case of a message and a cause */
#define HAIO_FAIL(why, message) \
    return std::unexpected(::Haio::Implementation::Error{::Haio::Implementation::ErrorCode::why, message})
