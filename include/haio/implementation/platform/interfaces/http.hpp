#pragma once

#include "haio/implementation/error.hpp"
#include "haio/implementation/platform/task.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * http both ways, as the platform does it: haio asking somebody else for bytes, and
 * somebody asking haio. the desktop answers with asio and beast; nothing here knows.
 */
namespace Haio::Implementation::Interfaces {

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
Platform::Task<Result<HttpResponse>> httpGet(HttpRequest request);

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

using HttpHandler = std::function<Platform::Task<HttpServerResponse>(HttpServerRequest)>;

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
Platform::Task<void> serveHttp(HttpServerOptions options, HttpHandler handler);

///@}

}
