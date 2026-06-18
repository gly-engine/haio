#include <haio_cdn.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/url/parse.hpp>

#include <algorithm>
#include <iostream>
#include <numeric>
#include <string_view>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

namespace {

std::string toString(std::string_view value) {
    return {value.begin(), value.end()};
}

std::string joinSegments(const std::vector<std::string>& segments, size_t first) {
    return std::accumulate(
        segments.begin() + static_cast<std::ptrdiff_t>(first),
        segments.end(),
        std::string{},
        [](std::string out, const std::string& segment) {
            if (!out.empty()) out.push_back('/');
            out += segment;
            return out;
        }
    );
}

struct Route {
    std::string bucket;
    std::string path;
    std::string query;
};

Route parseRoute(std::string_view target) {
    auto parsed = boost::urls::parse_origin_form(target);
    if (!parsed) throw std::runtime_error("invalid request target: " + parsed.error().message());

    const auto encodedPath = toString(parsed->encoded_path());
    constexpr std::string_view prefix = "/cdn/";
    if (!encodedPath.starts_with(prefix)) throw std::runtime_error("expected /cdn/<bucket>/<path>");

    std::vector<std::string> segments;
    for (auto segment : parsed->segments()) segments.emplace_back(segment);
    if (segments.empty() || segments.front() != "cdn") throw std::runtime_error("expected /cdn/<bucket>/<path>");

    const auto tail = std::string_view(encodedPath).substr(prefix.size());
    const auto query = parsed->has_query() ? toString(parsed->encoded_query()) : std::string{};
    if (tail.find('/') == std::string_view::npos) {
        return Route{"file", segments.size() > 1 ? segments[1] : std::string{}, query};
    }

    auto bucket = segments.size() > 1 ? segments[1] : std::string{};
    auto rest = segments.size() > 2 ? joinSegments(segments, 2) : std::string{};
    if (bucket.empty()) bucket = "file";
    return Route{std::move(bucket), std::move(rest), query};
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
