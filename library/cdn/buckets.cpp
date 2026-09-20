#include <haio_cdn.hpp>

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
#include <fstream>
#include <string_view>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace urls = boost::urls;
using tcp = asio::ip::tcp;

namespace {

constexpr auto httpTimeout = std::chrono::seconds(15);
constexpr int maxRedirects = 5;

std::string toString(std::string_view value) {
    return {value.begin(), value.end()};
}

std::string ensureSlash(std::string value) {
    if (value.empty() || value.front() != '/') value.insert(value.begin(), '/');
    return value;
}

std::filesystem::path safeJoin(const std::filesystem::path& root, std::string_view rawPath) {
    std::filesystem::path rel(rawPath);
    if (rel.is_absolute()) rel = rel.relative_path();

    std::filesystem::path clean;
    for (const auto& part : rel) {
        if (part == "." || part.empty()) continue;
        if (part == "..") throw std::runtime_error("path traversal is not allowed");
        clean /= part;
    }
    return root / clean;
}

/** a file bucket names a directory the way an http one names a host: "/abs" or "./rel" */
std::filesystem::path fileBucketRoot(const Haio::Cdn::BucketConfig& bucket) {
    if (bucket.endpoint.empty()) {
        throw std::runtime_error("file bucket \"" + bucket.name + "\" needs an endpoint starting with / or ./");
    }
    if (!bucket.endpoint.starts_with('/') && !bucket.endpoint.starts_with("./")) {
        throw std::runtime_error("file bucket endpoint must start with / or ./, got: " + bucket.endpoint);
    }
    return std::filesystem::path(bucket.endpoint);
}

Haio::Blob readFileBlob(const Haio::Cdn::BucketConfig& bucket, std::string path) {
    const auto fullPath = safeJoin(fileBucketRoot(bucket), path);
    std::ifstream in(fullPath, std::ios::binary);
    if (!in) throw std::runtime_error("file not found: " + fullPath.string());

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto format = Haio::formatFromExtension(fullPath.string());
    return Haio::Blob{format, std::string(Haio::contentTypeFor(format)), fullPath.string(), std::move(data)};
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
        throw std::runtime_error("unsupported http bucket scheme: " + toString(url.scheme()));
    }
}

std::string servicePort(const urls::url_view_base& url) {
    if (url.has_port()) return toString(url.port());
    return url.scheme() == "https" ? "443" : "80";
}

std::string requestTarget(const urls::url_view_base& url) {
    auto target = toString(url.encoded_path());
    if (target.empty()) target = "/";
    if (url.has_query()) {
        target.push_back('?');
        target += toString(url.encoded_query());
    }
    return target;
}

std::string withDefaultHttpScheme(std::string endpoint) {
    if (!endpoint.starts_with("http://") && !endpoint.starts_with("https://")) {
        endpoint.insert(0, "http://");
    }
    return endpoint;
}

urls::url parseEndpoint(std::string endpoint, std::string_view path) {
    auto normalized = withDefaultHttpScheme(std::move(endpoint));
    auto parsed = urls::parse_uri(normalized);
    if (!parsed) throw std::runtime_error("invalid http bucket endpoint: " + parsed.error().message());

    urls::url url(*parsed);
    requireHttpScheme(url);
    url.set_path(joinUrlPath(url.path(), path));
    return url;
}

/** without an endpoint the bucket reads the upstream off the url: <scheme>/<host>/<path> */
urls::url openBucketTarget(std::string_view path) {
    const auto slash = path.find('/');
    if (slash == std::string_view::npos) {
        throw std::runtime_error("open http bucket expects /cdn/<bucket>/<scheme>/<host>/<path>");
    }

    const auto scheme = path.substr(0, slash);
    if (scheme != "http" && scheme != "https") {
        throw std::runtime_error("open http bucket expects a http or https scheme, got: " + toString(scheme));
    }
    return parseEndpoint(toString(scheme) + "://" + toString(path.substr(slash + 1)), {});
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

    const auto host = toString(url.host());
    const auto results = co_await resolver.async_resolve(host, servicePort(url), asio::use_awaitable);
    const auto hostHeader = toString(url.encoded_host_and_port());
    const auto target = requestTarget(url);

    http::response<http::vector_body<uint8_t>> res;
    beast::error_code ec;

    if (url.scheme() == "https") {
        beast::ssl_stream<beast::tcp_stream> stream(executor, tlsContext());
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            throw std::runtime_error("could not set sni for " + host);
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
    if (!ref) throw std::runtime_error("upstream sent an invalid redirect: " + toString(location));

    urls::url next;
    if (const auto resolved = urls::resolve(from, *ref, next); !resolved) {
        throw std::runtime_error("could not resolve redirect: " + toString(location));
    }

    requireHttpScheme(next);
    return next;
}

asio::awaitable<Haio::Blob> fetchHttp(urls::url url, std::string pathForFormat) {
    for (int hop = 0;; hop++) {
        auto res = co_await request(url);
        const auto status = res.result_int();

        if (status >= 300 && status < 400) {
            if (hop >= maxRedirects) {
                throw std::runtime_error("upstream redirected more than " + std::to_string(maxRedirects) + " times");
            }
            const auto location = res[http::field::location];
            if (location.empty()) {
                throw std::runtime_error("upstream sent " + std::to_string(status) + " without a location header");
            }
            url = redirectTarget(url, std::string_view(location.data(), location.size()));
            continue;
        }

        if (status < 200 || status >= 300) {
            throw std::runtime_error("http bucket returned status " + std::to_string(status));
        }

        const auto format = Haio::formatFromExtension(pathForFormat.empty() ? requestTarget(url) : pathForFormat);
        co_return Haio::Blob{format, std::string(res[http::field::content_type]), std::move(pathForFormat), std::move(res.body())};
    }
}

}

namespace Haio::Cdn {

asio::awaitable<Blob> fetchBucket(const Config& config, std::string bucketName, std::string path) {
    const auto it = config.buckets.find(bucketName);
    if (it == config.buckets.end()) throw std::runtime_error("unknown bucket: " + bucketName);

    const auto& bucket = it->second;
    if (bucket.type == "file" || bucket.type.empty()) {
        co_return readFileBlob(bucket, std::move(path));
    }

    if (bucket.type == "http") {
        if (bucket.endpoint.empty()) {
            auto url = openBucketTarget(path);
            auto forFormat = requestTarget(url);
            co_return co_await fetchHttp(std::move(url), std::move(forFormat));
        }
        co_return co_await fetchHttp(parseEndpoint(bucket.endpoint, path), std::move(path));
    }

    if (bucket.type == "s3") {
        if (bucket.endpoint.empty()) {
            throw std::runtime_error("s3 bucket needs endpoint/base_url for now");
        }
        co_return co_await fetchHttp(parseEndpoint(bucket.endpoint, path), std::move(path));
    }

    throw std::runtime_error("unsupported bucket type: " + bucket.type);
}

}
