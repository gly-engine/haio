#include <haio_cdn.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <iostream>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

namespace {

std::string urlDecode(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] == '%' && i + 2 < input.size()) {
            auto hex = [](char c) -> int {
                if ('0' <= c && c <= '9') return c - '0';
                if ('a' <= c && c <= 'f') return c - 'a' + 10;
                if ('A' <= c && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            const int hi = hex(input[i + 1]);
            const int lo = hex(input[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(input[i] == '+' ? ' ' : input[i]);
    }
    return out;
}

struct Route {
    std::string bucket;
    std::string path;
    std::string query;
};

Route parseRoute(std::string_view target) {
    const auto q = target.find('?');
    auto path = target.substr(0, q);
    auto query = q == std::string_view::npos ? std::string_view{} : target.substr(q + 1);

    constexpr std::string_view prefix = "/cdn/";
    if (!path.starts_with(prefix)) throw std::runtime_error("expected /cdn/<bucket>/<path>");
    path.remove_prefix(prefix.size());

    const auto slash = path.find('/');
    if (slash == std::string_view::npos) {
        return Route{"file", urlDecode(path), std::string(query)};
    }

    auto bucket = urlDecode(path.substr(0, slash));
    auto rest = urlDecode(path.substr(slash + 1));
    if (bucket.empty()) bucket = "file";
    return Route{std::move(bucket), std::move(rest), std::string(query)};
}

bool hasTokenKind(const std::vector<Haio::Token>& tokens, Haio::TokenKind kind) {
    return std::ranges::any_of(tokens, [kind](const Haio::Token& token) { return token.kind == kind; });
}

bool hasImageTransform(const std::vector<Haio::Token>& tokens) {
    return std::ranges::any_of(tokens, [](const Haio::Token& token) {
        return token.kind == Haio::TokenKind::Crop || token.kind == Haio::TokenKind::Resize || token.kind == Haio::TokenKind::Radius;
    });
}

http::response<http::vector_body<uint8_t>> makeResponse(http::status status, std::string_view contentType, std::vector<uint8_t> body) {
    http::response<http::vector_body<uint8_t>> res{status, 11};
    res.set(http::field::server, "haio-cdn");
    res.set(http::field::content_type, contentType);
    res.body() = std::move(body);
    res.prepare_payload();
    return res;
}

http::response<http::vector_body<uint8_t>> makeText(http::status status, std::string text) {
    return makeResponse(status, "text/plain; charset=utf-8", std::vector<uint8_t>(text.begin(), text.end()));
}

asio::awaitable<http::response<http::vector_body<uint8_t>>> handleRequest(const Haio::Cdn::Config& config, http::request<http::string_body> req) {
    if (req.method() != http::verb::get) {
        co_return makeText(http::status::method_not_allowed, "method not allowed\n");
    }

    try {
        const auto route = parseRoute(req.target());
        auto blob = co_await Haio::Cdn::fetchBucket(config, route.bucket, route.path);
        auto tokens = Haio::parseQueryTokens(route.query);

        if (!tokens.empty()) {
            Haio::Pipeline pipeline;
            pipeline |= Haio::Tokens::Source(route.bucket, route.path);
            for (auto token : tokens) pipeline |= std::move(token);

            if (hasImageTransform(pipeline.tokens()) && !hasTokenKind(pipeline.tokens(), Haio::TokenKind::Encode)) {
                const auto fallback = blob.format == Haio::Format::RAW ? Haio::Format::PNG : blob.format;
                pipeline |= Haio::Tokens::Encode(fallback == Haio::Format::PPM ? Haio::Format::PNG : fallback);
            }

            blob = Haio::runPipeline(std::move(blob), pipeline);
        }

        co_return makeResponse(http::status::ok, blob.contentType.empty() ? Haio::contentTypeFor(blob.format) : blob.contentType, std::move(blob.data));
    } catch (const std::exception& err) {
        co_return makeText(http::status::bad_request, std::string(err.what()) + "\n");
    }
}

asio::awaitable<void> session(tcp::socket socket, Haio::Cdn::Config config) {
    beast::flat_buffer buffer;
    try {
        for (;;) {
            http::request<http::string_body> req;
            co_await http::async_read(socket, buffer, req, asio::use_awaitable);
            const bool close = req.need_eof();
            auto res = co_await handleRequest(config, std::move(req));
            res.keep_alive(!close);
            co_await http::async_write(socket, res, asio::use_awaitable);
            if (close) break;
        }
    } catch (const std::exception&) {
    }

    beast::error_code ec;
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

asio::awaitable<void> listener(Haio::Cdn::Config config) {
    auto executor = co_await asio::this_coro::executor;
    tcp::resolver resolver(executor);
    const auto resolved = co_await resolver.async_resolve(config.host, std::to_string(config.port), asio::use_awaitable);
    tcp::acceptor acceptor(executor, *resolved.begin());
    std::cout << "haio cdn listening on http://" << config.host << ':' << config.port << "\n";

    for (;;) {
        auto socket = co_await acceptor.async_accept(asio::use_awaitable);
        asio::co_spawn(executor, session(std::move(socket), config), asio::detached);
    }
}

}

namespace Haio::Cdn {

asio::awaitable<void> runServer(Config config) {
    co_await listener(std::move(config));
}

}
