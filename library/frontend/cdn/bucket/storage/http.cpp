#include <bucket/bucket.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <chrono>
#include <iostream>
#include <string_view>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace urls = boost::urls;
using tcp = asio::ip::tcp;

namespace {

constexpr auto httpTimeout = std::chrono::seconds(15);
constexpr int maxRedirects = 5;

std::string ensureSlash(std::string value) {
    if (value.empty() || value.front() != '/') value.insert(value.begin(), '/');
    return value;
}

std::string joinUrlPath(std::string_view prefix, std::string_view path) {
    if (prefix.empty() || prefix == "/") return ensureSlash(std::string(path));
    if (path.empty()) return ensureSlash(std::string(prefix));

    std::string out(prefix);
    if (!out.ends_with('/')) out.push_back('/');
    if (path.starts_with('/')) path.remove_prefix(1);
    out.append(path);
    return ensureSlash(std::move(out));
}

void requireHttpScheme(const urls::url_view_base& url) {
    if (url.scheme() != "http" && url.scheme() != "https") {
        throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::InvalidInput, "unsupported scheme");
    }
}

std::string servicePort(const urls::url_view_base& url) {
    if (url.has_port()) return std::string(url.port());
    return url.scheme() == "https" ? "443" : "80";
}

std::string requestTarget(const urls::url_view_base& url) {
    auto target = std::string(url.encoded_path());
    if (target.empty()) target = "/";
    if (url.has_query()) {
        target.push_back('?');
        target += std::string(url.encoded_query());
    }
    return target;
}

urls::url parseEndpoint(std::string endpoint, std::string_view path) {
    const auto parsed = urls::parse_uri(endpoint);
    if (!parsed) {
        std::cerr << "invalid bucket endpoint: " << endpoint << "\n";
        throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::InvalidInput, "this bucket is misconfigured");
    }

    urls::url url(*parsed);
    // s3 is plain https until it learns to sign
    if (url.scheme() == "s3") url.set_scheme("https");
    requireHttpScheme(url);
    url.set_path(joinUrlPath(url.path(), path));
    return url;
}

/**
 * an open bucket reads the upstream off the request: "https://\*" leaves only the host
 * to the url, "//\*" leaves the scheme as well.
 *
 * this is deliberate and meant for testing, which is why runServer warns about every
 * open bucket it finds at startup. it will fetch whatever host the caller names,
 * including ones only this machine can reach, so a deployment that faces anyone but
 * you should not configure one.
 */
urls::url openBucketTarget(const Haio::Cdn::BucketConfig& bucket, std::string_view path) {
    if (!bucket.scheme.empty()) {
        const auto scheme = bucket.scheme == "s3" ? std::string("https") : bucket.scheme;
        if (path.empty()) {
            throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::InvalidInput, "open bucket expects /cdn/" + bucket.name + "/<host>/<path>");
        }
        return parseEndpoint(scheme + "://" + std::string(path), {});
    }

    const auto slash = path.find('/');
    if (slash == std::string_view::npos) {
        throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::InvalidInput, "open bucket expects /cdn/" + bucket.name + "/<scheme>/<host>/<path>");
    }

    const auto scheme = path.substr(0, slash);
    if (scheme != "http" && scheme != "https") {
        throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::InvalidInput, "open bucket expects a http or https scheme, got: " + std::string(scheme));
    }
    return parseEndpoint(std::string(scheme) + "://" + std::string(path.substr(slash + 1)), {});
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

template <typename Stream>
asio::awaitable<http::response<http::vector_body<uint8_t>>> exchange(Stream& stream, std::string_view hostHeader, std::string_view target) {
    http::request<http::empty_body> req{http::verb::get, target, 11};
    req.set(http::field::host, hostHeader);
    req.set(http::field::user_agent, "haio-cdn");

    co_await http::async_write(stream, req, asio::use_awaitable);

    beast::flat_buffer buffer;
    http::response<http::vector_body<uint8_t>> res;
    co_await http::async_read(stream, buffer, res, asio::use_awaitable);
    co_return res;
}

asio::awaitable<http::response<http::vector_body<uint8_t>>> request(const urls::url& url) {
    auto executor = co_await asio::this_coro::executor;
    tcp::resolver resolver(executor);

    const auto host = std::string(url.host());
    const auto results = co_await resolver.async_resolve(host, servicePort(url), asio::use_awaitable);
    const auto hostHeader = std::string(url.encoded_host_and_port());
    const auto target = requestTarget(url);

    http::response<http::vector_body<uint8_t>> res;
    beast::error_code ec;

    if (url.scheme() == "https") {
        beast::ssl_stream<beast::tcp_stream> stream(executor, tlsContext());
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            // the host is the bucket's upstream, which the caller has no business
            // learning from an error; it goes to the log instead
            std::cerr << "could not set sni for " << host << "\n";
            throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::Internal, "could not start a secure connection");
        }
        stream.set_verify_callback(asio::ssl::host_name_verification(host));

        beast::get_lowest_layer(stream).expires_after(httpTimeout);
        co_await beast::get_lowest_layer(stream).async_connect(results, asio::use_awaitable);
        co_await stream.async_handshake(asio::ssl::stream_base::client, asio::use_awaitable);

        res = co_await exchange(stream, hostHeader, target);

        // a truncated shutdown is the normal case for servers that just close
        co_await stream.async_shutdown(asio::redirect_error(asio::use_awaitable, ec));
    } else {
        beast::tcp_stream stream(executor);
        stream.expires_after(httpTimeout);
        co_await stream.async_connect(results, asio::use_awaitable);

        res = co_await exchange(stream, hostHeader, target);

        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    }

    co_return res;
}

/** a redirect may move between http and https in either direction */
urls::url redirectTarget(const urls::url& from, std::string_view location) {
    auto ref = urls::parse_uri_reference(location);
    if (!ref) {
        std::cerr << "upstream sent an invalid redirect: " << location << "\n";
        throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::Upstream, "upstream sent an invalid redirect");
    }

    urls::url next;
    if (const auto resolved = urls::resolve(from, *ref, next); !resolved) {
        std::cerr << "could not resolve redirect: " << location << "\n";
        throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::Upstream, "upstream sent a redirect that goes nowhere");
    }

    requireHttpScheme(next);
    return next;
}

asio::awaitable<Haio::Blob> fetchUrl(urls::url url, std::string pathForFormat) {
    for (int hop = 0;; hop++) {
        auto res = co_await request(url);
        const auto status = res.result_int();

        if (status >= 300 && status < 400) {
            if (hop >= maxRedirects) {
                throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::Upstream, "upstream redirected more than " + std::to_string(maxRedirects) + " times");
            }
            const auto location = res[http::field::location];
            if (location.empty()) {
                throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::Upstream, "upstream sent " + std::to_string(status) + " without a location header");
            }
            url = redirectTarget(url, std::string_view(location.data(), location.size()));
            continue;
        }

        if (status < 200 || status >= 300) {
            throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::Upstream, "http bucket returned status " + std::to_string(status));
        }

        const auto contentType = res[http::field::content_type];
        const auto found = Haio::Detect(res.body());

        // the bytes win over what the server or the url claim, because those are the
        // two things that lie: gam creatives carry no extension and are often mislabelled
        auto format = found.format;
        if (format == Haio::Format::RAW) {
            format = Haio::formatFromContentType(std::string_view(contentType.data(), contentType.size()));
        }
        if (format == Haio::Format::RAW) {
            format = Haio::formatFromExtension(pathForFormat.empty() ? requestTarget(url) : pathForFormat);
        }

        co_return Haio::Blob{format, found.color, std::string(contentType), std::move(pathForFormat), std::move(res.body())};
    }
}

}

namespace Haio::Cdn::Bucket {

asio::awaitable<Blob> fetchHttp(const BucketConfig& bucket, std::string path) {
    if (bucket.open) {
        auto url = openBucketTarget(bucket, path);
        auto forFormat = requestTarget(url);
        co_return co_await fetchUrl(std::move(url), std::move(forFormat));
    }
    co_return co_await fetchUrl(parseEndpoint(bucket.url, path), std::move(path));
}

}
