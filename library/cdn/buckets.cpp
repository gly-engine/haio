#include <haio_cdn.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <fstream>
#include <sstream>

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
    std::string target = "/";
};

HttpTarget parseHttpTarget(std::string endpoint, std::string path) {
    if (endpoint.starts_with("http://")) endpoint.erase(0, 7);
    if (endpoint.starts_with("https://")) throw std::runtime_error("https buckets are not supported yet");

    const auto slash = endpoint.find('/');
    std::string hostPort = slash == std::string::npos ? endpoint : endpoint.substr(0, slash);
    std::string prefix = slash == std::string::npos ? std::string{} : endpoint.substr(slash);

    HttpTarget out;
    if (const auto colon = hostPort.find(':'); colon != std::string::npos) {
        out.host = hostPort.substr(0, colon);
        out.port = hostPort.substr(colon + 1);
    } else {
        out.host = hostPort;
    }
    out.target = ensureSlash(prefix + "/" + path);
    return out;
}

asio::awaitable<Haio::Blob> fetchHttp(std::string host, std::string port, std::string target, std::string pathForFormat) {
    auto executor = co_await asio::this_coro::executor;
    tcp::resolver resolver(executor);
    beast::tcp_stream stream(executor);

    auto results = co_await resolver.async_resolve(host, port, asio::use_awaitable);
    co_await stream.async_connect(results, asio::use_awaitable);

    http::request<http::empty_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host);
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
        auto host = path.substr(0, slash);
        auto target = path.substr(slash);
        co_return co_await fetchHttp(std::move(host), "80", ensureSlash(target), target);
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
        co_return co_await fetchHttp(std::move(target.host), std::move(target.port), std::move(target.target), path);
    }

    if (bucket.type == "s3") {
        if (bucket.endpoint.empty()) {
            throw std::runtime_error("s3 bucket needs endpoint/base_url for now");
        }
        auto target = parseHttpTarget(bucket.endpoint, path);
        co_return co_await fetchHttp(std::move(target.host), std::move(target.port), std::move(target.target), path);
    }

    throw std::runtime_error("unsupported bucket type: " + bucket.type);
}

}
