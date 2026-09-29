#include <haio/internal/platform/asio.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/system/system_error.hpp>

#include <haio_url.hpp>

#include <chrono>
#include <iostream>
#include <string_view>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

namespace {

constexpr auto httpTimeout = std::chrono::seconds(15);
constexpr int maxRedirects = 5;

/** carries why a hop failed out of the asio side, where only exceptions travel well */
struct Refused : std::runtime_error {
    Haio::ErrorCode code;
    Refused(Haio::ErrorCode why, const std::string& what) : std::runtime_error(what), code(why) {}
};

void requireHttpScheme(const Haio::Url& url) {
    if (url.scheme != "http" && url.scheme != "https") {
        throw Refused(Haio::ErrorCode::InvalidInput, "unsupported scheme");
    }
}

std::string servicePort(const Haio::Url& url) {
    if (!url.port.empty()) return url.port;
    return url.scheme == "https" ? "443" : "80";
}

/** shared across requests: building it reloads the whole ca store every time */
asio::ssl::context& tlsContext() {
    static asio::ssl::context context = [] {
        asio::ssl::context created(asio::ssl::context::tls_client);
        created.set_default_verify_paths();
        created.set_verify_mode(asio::ssl::verify_peer);
        return created;
    }();
    return context;
}

using Response = http::response<http::vector_body<uint8_t>>;

template <typename Stream>
asio::awaitable<Response> exchange(Stream& stream, std::string_view hostHeader, std::string_view target,
                                   const Haio::Platform::Headers& headers) {
    http::request<http::empty_body> req{http::verb::get, target, 11};
    req.set(http::field::host, hostHeader);
    req.set(http::field::user_agent, "haio");
    for (const auto& [name, value] : headers) req.set(name, value);

    co_await http::async_write(stream, req, asio::use_awaitable);

    beast::flat_buffer buffer;
    Response res;
    co_await http::async_read(stream, buffer, res, asio::use_awaitable);
    co_return res;
}

asio::awaitable<Response> request(const Haio::Url& url, const Haio::Platform::Headers& headers) {
    auto executor = co_await asio::this_coro::executor;
    tcp::resolver resolver(executor);

    const auto host = url.hostName();
    const auto results = co_await resolver.async_resolve(host, servicePort(url), asio::use_awaitable);
    const auto hostHeader = url.authority();
    const auto target = url.target();

    Response res;
    beast::error_code ec;

    if (url.scheme == "https") {
        beast::ssl_stream<beast::tcp_stream> stream(executor, tlsContext());
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            // the host may be a bucket's upstream, which the caller has no business
            // learning from an error; it goes to the log instead
            std::cerr << "could not set sni for " << host << "\n";
            throw Refused(Haio::ErrorCode::Internal, "could not start a secure connection");
        }
        stream.set_verify_callback(asio::ssl::host_name_verification(host));

        beast::get_lowest_layer(stream).expires_after(httpTimeout);
        co_await beast::get_lowest_layer(stream).async_connect(results, asio::use_awaitable);
        co_await stream.async_handshake(asio::ssl::stream_base::client, asio::use_awaitable);

        res = co_await exchange(stream, hostHeader, target, headers);

        // a truncated shutdown is the normal case for servers that just close
        co_await stream.async_shutdown(asio::redirect_error(asio::use_awaitable, ec));
    } else {
        beast::tcp_stream stream(executor);
        stream.expires_after(httpTimeout);
        co_await stream.async_connect(results, asio::use_awaitable);

        res = co_await exchange(stream, hostHeader, target, headers);

        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    }

    co_return res;
}

/** a redirect may move between http and https in either direction */
Haio::Url redirectTarget(const Haio::Url& from, std::string_view location) {
    auto next = from.resolve(location);
    if (!next) {
        std::cerr << "upstream sent an invalid redirect: " << location << "\n";
        throw Refused(Haio::ErrorCode::Upstream, "upstream sent an invalid redirect");
    }

    requireHttpScheme(*next);
    return *std::move(next);
}

asio::awaitable<Haio::Platform::HttpResponse> follow(Haio::Url url, Haio::Platform::Headers headers) {
    for (int hop = 0;; hop++) {
        auto res = co_await request(url, headers);
        const auto status = res.result_int();

        if (status >= 300 && status < 400) {
            if (hop >= maxRedirects) {
                throw Refused(Haio::ErrorCode::Upstream, "upstream redirected more than " + std::to_string(maxRedirects) + " times");
            }
            const auto location = res[http::field::location];
            if (location.empty()) {
                throw Refused(Haio::ErrorCode::Upstream, "upstream sent " + std::to_string(status) + " without a location header");
            }
            // the signature was made for the url that redirected, and means nothing at
            // the next one, so it is dropped rather than replayed somewhere it does not
            // belong
            headers.clear();
            url = redirectTarget(url, std::string_view(location.data(), location.size()));
            continue;
        }

        co_return Haio::Platform::HttpResponse{
            static_cast<int>(status),
            std::string(res[http::field::content_type]),
            std::move(res.body()),
            url.str(),
        };
    }
}

}

namespace Haio::Platform {

Task<Result<HttpResponse>> httpGet(HttpRequest request) {
    auto parsed = Haio::Url::parse(request.url);
    if (!parsed || parsed->scheme.empty()) {
        std::cerr << "invalid url: " << request.url << "\n";
        co_return std::unexpected(Error{ErrorCode::InvalidInput, "not a url"});
    }

    /**
     * every way a fetch can fail ends up as an Error here, so nothing above the
     * platform ever has to know what a boost::system::system_error is.
     */
    try {
        requireHttpScheme(*parsed);
        co_return co_await Desktop::await(follow(*std::move(parsed), std::move(request.headers)));
    } catch (const Refused& err) {
        co_return std::unexpected(Error{err.code, err.what()});
    } catch (const boost::system::system_error& err) {
        if (err.code() == beast::error::timeout) {
            co_return std::unexpected(Error{ErrorCode::Timeout, "upstream timed out"});
        }
        co_return std::unexpected(Error{ErrorCode::Upstream, "upstream unreachable: " + std::string(err.code().message())});
    } catch (const std::exception& err) {
        co_return std::unexpected(Error{ErrorCode::Internal, err.what()});
    }
}

}
