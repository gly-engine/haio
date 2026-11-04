#include <haio_platform.hpp>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#else
#include <unistd.h>
#endif

namespace {

char** environment() {
#if defined(_WIN32)
    return _environ;
#else
    return environ;
#endif
}

std::string fromEnv(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : "";
}

std::string_view reasonFor(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 415: return "Unsupported Media Type";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 504: return "Gateway Timeout";
        default: break;
    }
    return status < 400 ? "OK" : status < 500 ? "Client Error" : "Server Error";
}

void readHeaders(Haio::Platform::HttpServerRequest& req) {
    for (char** entry = environment(); entry && *entry; entry++) {
        const std::string_view line = *entry;
        const auto equals = line.find('=');
        if (equals == std::string_view::npos || !line.starts_with("HTTP_")) continue;

        std::string name;
        for (const char c : line.substr(5, equals - 5)) {
            name.push_back(c == '_' ? '-' : static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        req.headers[std::move(name)] = std::string(line.substr(equals + 1));
    }
}

Haio::Platform::HttpServerResponse failed(std::string message) {
    message += '\n';
    return {500, {{"content-type", "text/plain; charset=utf-8"}}, {message.begin(), message.end()}};
}

}

namespace Haio::Platform {

Task<void> serveHttpCgi(HttpHandler handler) {
#if defined(_WIN32)
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    HttpServerRequest req;
    req.method = fromEnv("REQUEST_METHOD");
    if (req.method.empty()) req.method = "GET";
    req.client = fromEnv("REMOTE_ADDR");
    readHeaders(req);

    req.target = "/cdn";
    if (const auto query = fromEnv("QUERY_STRING"); !query.empty()) req.target += '?' + query;

    const bool headOnly = req.method == "HEAD";

    HttpServerResponse res;
    try {
        res = co_await handler(std::move(req));
    } catch (const std::exception& err) {
        std::cerr << "error: " << err.what() << "\n";
        res = failed(err.what());
    }

    std::string head = "Status: " + std::to_string(res.status) + ' ' + std::string(reasonFor(res.status)) + "\r\n";
    for (const auto& [name, value] : res.headers) head += name + ": " + value + "\r\n";
    head += "content-length: " + std::to_string(res.body.size()) + "\r\n\r\n";

    std::fwrite(head.data(), 1, head.size(), stdout);
    if (!headOnly && !res.body.empty()) std::fwrite(res.body.data(), 1, res.body.size(), stdout);
    std::fflush(stdout);
}

}
