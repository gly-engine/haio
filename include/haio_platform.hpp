#pragma once

#include "haio_codec.hpp"
#include "haio_task.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/**
 * everything haio needs from the machine it runs on, and nothing it does with it.
 *
 * one directory under library/platform answers all of this, picked by HAIO_PLATFORM
 * at configure time. desktop does it with asio, beast and a socket; a browser build
 * would do the client half with fetch() and leave the server half out, since a page
 * does not listen on a port. nothing outside library/platform includes asio.
 *
 * the split follows who opens the connection:
 *
 *   runtime   the loop everything runs on
 *   client    haio asking somebody else for bytes: http, and redis on the desktop
 *   server    somebody asking haio: the cdn's listener
 */
namespace Haio::Platform {

/** @name runtime */
///@{

/** starts a task on the loop with nobody waiting for it; what it throws is logged */
void spawn(Task<void> task);

/**
 * drives the loop until main is done or stop() is called, and rethrows whatever main
 * threw. the loop is the platform's own, so what spawn() started earlier runs too.
 */
void run(Task<void> main);

/** makes run() return, from inside the loop or from a signal */
void stop();

/** SIGINT and SIGTERM stop the loop rather than the process, so run() returns */
void stopOnSignal();

/** resumes the caller once the time has passed, without holding up the loop */
Task<void> sleep(std::chrono::milliseconds duration);

/**
 * a task's answer, for code that has nothing better to do than wait for it: the
 * convert command line, a test. it drives the loop, so it is never called from
 * inside a task.
 */
template <typename T>
T blockOn(Task<T> task) {
    std::optional<T> out;
    run([](Task<T> inner, std::optional<T>& into) -> Task<void> {
        into.emplace(co_await std::move(inner));
    }(std::move(task), out));
    if (!out) throw std::runtime_error("the loop was stopped before the task finished");
    return *std::move(out);
}

///@}

/** @name client */
///@{

/** header names are compared as given, so callers write them in lowercase */
using Headers = std::map<std::string, std::string>;

struct HttpRequest {
    std::string url;
    Headers headers;
};

/**
 * what came back from the last hop, whatever its status: deciding that a 404 is a
 * failure is the caller's business, not the transport's.
 */
struct HttpResponse {
    int status = 0;
    std::string contentType;
    std::vector<uint8_t> body;
    /** where the redirects ended, which is what the body's name should come from */
    std::string url;
};

/**
 * a GET, with redirects followed across http and https in either direction.
 *
 * the headers go to the first hop only. they are how s3 carries its signature, and a
 * signature made for one url means nothing at the next one, so they are dropped
 * rather than replayed somewhere they do not belong. a browser follows redirects on
 * its own and never shows them, which is why this is the platform's job and not the
 * caller's.
 *
 * the error is Timeout when the upstream was too slow, Upstream when it could not be
 * reached or redirected badly, and InvalidInput for a url that is not http at all.
 */
Task<Result<HttpResponse>> httpGet(HttpRequest request);

///@}

/** @name server */
///@{

struct HttpServerRequest {
    std::string method;
    std::string target;
    /** names in lowercase, so a lookup does not depend on how the client spelled them */
    std::map<std::string, std::string> headers;
    /** the address the connection came from; empty when the socket would not say */
    std::string client;

    std::string header(const std::string& name) const {
        const auto found = headers.find(name);
        return found == headers.end() ? std::string{} : found->second;
    }
};

struct HttpServerResponse {
    int status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<uint8_t> body;
};

using HttpHandler = std::function<Task<HttpServerResponse>(HttpServerRequest)>;

struct HttpServerOptions {
    std::string host = "0.0.0.0";
    unsigned short port = 8080;
    /** the header every response carries as server */
    std::string name = "haio";
    /** called once the socket is bound, which is the moment "listening" is true */
    std::function<void()> ready;
};

/**
 * accepts connections until the loop stops, one handler call per request.
 *
 * keep alive, HEAD and content-length are answered here, since they are the same for
 * every handler: a HEAD reaches the handler as a HEAD, and whatever body it returns
 * is measured and then left unsent.
 */
Task<void> serveHttp(HttpServerOptions options, HttpHandler handler);

///@}

}
