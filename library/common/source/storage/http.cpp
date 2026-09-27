#include <haio/internal/source/storage.hpp>

#include <haio_url.hpp>

#include <iostream>
#include <string_view>

namespace {

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

/**
 * the endpoint with the path joined onto it. the scheme is not checked here: the
 * platform refuses anything that is not http, and an endpoint that got this far was
 * already read by resolveOrigin.
 */
Haio::Result<Haio::Url> parseEndpoint(const std::string& endpoint, std::string_view path) {
    auto url = Haio::Url::parse(endpoint);
    if (!url || url->scheme.empty()) {
        std::cerr << "invalid endpoint: " << endpoint << "\n";
        HAIO_FAIL(InvalidInput, "this bucket is misconfigured");
    }
    url->setPath(joinUrlPath(url->decodedPath(), path));
    return *std::move(url);
}

/**
 * an open origin reads the upstream off the path: "https://\*" leaves only the host
 * to the url, "//\*" leaves the scheme as well.
 *
 * this is deliberate and meant for testing, which is why runServer warns about every
 * open bucket it finds at startup. it will fetch whatever host the caller names,
 * including ones only this machine can reach, so a deployment that faces anyone but
 * you should not configure one.
 */
Haio::Result<Haio::Url> openTarget(const Haio::Source::Origin& origin, std::string_view path) {
    if (!origin.scheme.empty()) {
        if (path.empty()) HAIO_FAIL(InvalidInput, "open bucket expects /cdn/" + origin.name + "/<host>/<path>");
        return parseEndpoint(origin.scheme + "://" + std::string(path), {});
    }

    const auto slash = path.find('/');
    if (slash == std::string_view::npos) {
        HAIO_FAIL(InvalidInput, "open bucket expects /cdn/" + origin.name + "/<scheme>/<host>/<path>");
    }

    const auto scheme = path.substr(0, slash);
    if (scheme != "http" && scheme != "https") {
        HAIO_FAIL(InvalidInput, "open bucket expects a http or https scheme, got: " + std::string(scheme));
    }
    return parseEndpoint(std::string(scheme) + "://" + std::string(path.substr(slash + 1)), {});
}

}

namespace Haio::Source {

Task<Result<Blob>> fetchUrlWith(std::string url, std::string pathForFormat, Platform::Headers headers) {
    auto res = co_await Platform::httpGet(Platform::HttpRequest{std::move(url), std::move(headers)});
    if (!res) co_return std::unexpected(res.error());
    if (res->status < 200 || res->status >= 300) {
        co_return std::unexpected(Error{ErrorCode::Upstream, "upstream returned status " + std::to_string(res->status)});
    }

    // with no path of its own, the body is named by wherever the redirects ended
    if (pathForFormat.empty()) {
        const auto landed = Url::parse(res->url);
        pathForFormat = landed ? landed->target() : res->url;
    }
    co_return blobFrom(std::move(res->body), std::move(pathForFormat), res->contentType);
}

Task<Result<Blob>> fetchHttp(const Origin& origin, std::string path) {
    if (origin.open) {
        const auto url = openTarget(origin, path);
        if (!url) co_return std::unexpected(url.error());
        co_return co_await fetchUrlWith(url->str(), url->target(), {});
    }

    const auto url = parseEndpoint(origin.url, path);
    if (!url) co_return std::unexpected(url.error());
    co_return co_await fetchUrlWith(url->str(), std::move(path), {});
}

}
