#include <haio_cdn.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <fstream>
#include <string_view>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

namespace {

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

Haio::Blob readFileBlob(const Haio::Cdn::BucketConfig& bucket, std::string path) {
    const auto fullPath = safeJoin(bucket.root, path);
    std::ifstream in(fullPath, std::ios::binary);
    if (!in) throw std::runtime_error("file not found: " + fullPath.string());

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto format = Haio::formatFromExtension(fullPath.string());
    return Haio::Blob{format, std::string(Haio::contentTypeFor(format)), fullPath.string(), std::move(data)};
}

struct HttpTarget {
    std::string host;
    std::string port = "80";
    std::string authority;
    std::string target = "/";
};

std::string toString(std::string_view value) {
    return {value.begin(), value.end()};
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

std::string requestTarget(const boost::urls::url& url) {
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

HttpTarget parseHttpTarget(std::string endpoint, std::string path) {
    auto normalized = withDefaultHttpScheme(std::move(endpoint));
    auto parsed = boost::urls::parse_uri(normalized);
    if (!parsed) throw std::runtime_error("invalid http bucket endpoint: " + parsed.error().message());

    boost::urls::url url(*parsed);
    if (url.scheme() == "https") throw std::runtime_error("https buckets are not supported yet");
    if (url.scheme() != "http") throw std::runtime_error("unsupported http bucket scheme: " + toString(url.scheme()));

    url.set_path(joinUrlPath(url.path(), path));

    return HttpTarget{
        .host = toString(url.host()),
        .port = url.has_port() ? toString(url.port()) : "80",
        .authority = toString(url.encoded_host_and_port()),
        .target = requestTarget(url),
    };
}

asio::awaitable<Haio::Blob> fetchHttp(std::string host, std::string port, std::string authority, std::string target, std::string pathForFormat) {
    auto executor = co_await asio::this_coro::executor;
    tcp::resolver resolver(executor);
    beast::tcp_stream stream(executor);

    auto results = co_await resolver.async_resolve(host, port, asio::use_awaitable);
    co_await stream.async_connect(results, asio::use_awaitable);

    http::request<http::empty_body> req{http::verb::get, target, 11};
    req.set(http::field::host, authority.empty() ? host : authority);
    req.set(http::field::user_agent, "haio-cdn");

    co_await http::async_write(stream, req, asio::use_awaitable);

    beast::flat_buffer buffer;
    http::response<http::vector_body<uint8_t>> res;
    co_await http::async_read(stream, buffer, res, asio::use_awaitable);

    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    if (res.result_int() < 200 || res.result_int() >= 300) {
        throw std::runtime_error("http bucket returned status " + std::to_string(res.result_int()));
    }

    const auto format = Haio::formatFromExtension(pathForFormat.empty() ? target : pathForFormat);
    co_return Haio::Blob{format, std::string(res[http::field::content_type]), pathForFormat, std::move(res.body())};
}

}

namespace Haio::Cdn {

asio::awaitable<Blob> fetchBucket(const Config& config, std::string bucketName, std::string path) {
    if (bucketName == "http") {
        const auto slash = path.find('/');
        if (slash == std::string::npos) throw std::runtime_error("inline http bucket expects /cdn/http/host/path");
        auto target = parseHttpTarget(path, "");
        co_return co_await fetchHttp(std::move(target.host), std::move(target.port), std::move(target.authority), std::move(target.target), target.target);
    }

    auto it = config.buckets.find(bucketName);
    if (it == config.buckets.end()) {
        if (bucketName == "file") it = config.buckets.find("file");
        if (it == config.buckets.end()) throw std::runtime_error("unknown bucket: " + bucketName);
    }

    const auto& bucket = it->second;
    if (bucket.type == "file" || bucket.type.empty()) {
        co_return readFileBlob(bucket, std::move(path));
    }

    if (bucket.type == "http") {
        HttpTarget target;
        if (!bucket.endpoint.empty()) {
            target = parseHttpTarget(bucket.endpoint, path);
        } else {
            target.host = bucket.host;
            target.target = ensureSlash(bucket.prefix + "/" + path);
        }
        if (target.host.empty()) throw std::runtime_error("http bucket has no host");
        co_return co_await fetchHttp(std::move(target.host), std::move(target.port), std::move(target.authority), std::move(target.target), path);
    }

    if (bucket.type == "s3") {
        if (bucket.endpoint.empty()) {
            throw std::runtime_error("s3 bucket needs endpoint/base_url for now");
        }
        auto target = parseHttpTarget(bucket.endpoint, path);
        co_return co_await fetchHttp(std::move(target.host), std::move(target.port), std::move(target.authority), std::move(target.target), path);
    }

    throw std::runtime_error("unsupported bucket type: " + bucket.type);
}

}
