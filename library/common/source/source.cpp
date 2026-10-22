#include <haio/internal/source/storage.hpp>

#include <haio_url.hpp>

#include <cstdlib>
#include <stdexcept>

namespace {

std::string who(const Haio::Source::Origin& origin) {
    return "bucket \"" + origin.name + "\"";
}

/** file://relative keeps the authority as the first path segment, file:///absolute has none */
std::filesystem::path fileRoot(const Haio::Url& url, const Haio::Source::Origin& origin) {
    const auto host = url.hostName();
    const auto path = url.decodedPath();

    if (host.empty()) {
        if (path.empty() || path == "/") {
            throw std::runtime_error("file " + who(origin) + " has no directory in its url");
        }
        return std::filesystem::path(path);
    }
    return std::filesystem::path(host + path);
}

/** the same test fetchS3 makes before it signs, so the two cannot disagree */
bool willSign(const Haio::Source::Origin& origin) {
    const auto set = [](const char* name) {
        const char* value = std::getenv(name);
        return value && *value;
    };
    return (!origin.accessKey.empty() && !origin.secretKey.empty())
        || (set("AWS_ACCESS_KEY_ID") && set("AWS_SECRET_ACCESS_KEY"));
}

}

namespace Haio::Source {

void resolveOrigin(Origin& origin) {
    if (origin.url.empty()) {
        throw std::runtime_error(who(origin) + " has no url");
    }

    const auto parsed = Url::parse(origin.url);
    if (!parsed) {
        throw std::runtime_error(who(origin) + " has an invalid url: " + origin.url);
    }

    origin.scheme = parsed->scheme;
    origin.open = parsed->host == "*";

    if (origin.scheme == "file") {
        if (origin.open) {
            throw std::runtime_error("a file bucket cannot be open: " + origin.url);
        }
        origin.root = fileRoot(*parsed, origin);
        return;
    }

    if (origin.scheme == "s3") {
        // a signature is made for one host, so there is nothing sensible to sign for
        // an origin whose host arrives with the request
        if (origin.open) throw std::runtime_error("an s3 bucket cannot be open: " + origin.url);

        /**
         * the region is part of the signature, and nothing else: a bucket that will not
         * sign needs none. one that will is refused here rather than at its first
         * request, since signing with no region comes back as a plain 403 that says
         * nothing about the region being the problem.
         */
        if (origin.region.empty() && willSign(origin)) {
            throw std::runtime_error("s3 " + who(origin) + " signs its requests and has no region; "
                                     "write region = \"<name>\" in it");
        }
        return;
    }
    if (origin.scheme == "http" || origin.scheme == "https") return;

    // "//\*" carries no scheme on purpose: the request supplies it
    if (origin.scheme.empty()) {
        if (!origin.open) {
            throw std::runtime_error(who(origin) + " needs a scheme in its url: " + origin.url);
        }
        return;
    }

    throw std::runtime_error("unsupported url scheme \"" + origin.scheme + "\" in " + who(origin));
}

Task<Result<Blob>> fetchAny(const Origin& origin, std::string path) {
    if (origin.scheme == "file") co_return fetchFile(origin, std::move(path));
    if (origin.scheme == "s3") co_return co_await fetchS3(origin, std::move(path));
    co_return co_await fetchHttp(origin, std::move(path));
}

Task<Result<Blob>> fetch(const Origin& origin, std::string path) {
    // every failure a fetcher knows about is already a Result; this only keeps an
    // allocation that failed from taking the whole request down with it
    try {
        co_return co_await fetchAny(origin, std::move(path));
    } catch (const std::exception& err) {
        co_return std::unexpected(Error{ErrorCode::Internal, err.what()});
    }
}

Blob blobFrom(std::vector<uint8_t> data, std::string path, std::string_view contentType) {
    const auto found = Detect(data);

    auto format = found.format;
    if (format == Format::RAW) format = formatFromContentType(contentType);
    if (format == Format::RAW) format = formatFromExtension(path);

    // a format haio can name is described the way haio describes it; one it cannot is
    // left with whatever the server called it, which is all anybody knows
    auto type = format != Format::RAW ? std::string(contentTypeFor(format))
              : !contentType.empty()  ? std::string(contentType)
                                      : Blob{}.contentType;
    return Blob{format, found.color, std::move(type), std::move(path), std::move(data)};
}

bool isRemoteUri(std::string_view text) {
    return text.starts_with("http://") || text.starts_with("https://");
}

Task<Result<Blob>> fetchUri(std::string uri) {
    if (!isRemoteUri(uri)) {
        co_return std::unexpected(Error{ErrorCode::InvalidInput, "expected an http or https url: " + uri});
    }
    // the url is the whole file, so there is no origin to resolve and nothing to join
    co_return co_await fetchUrlWith(std::move(uri), {}, {});
}

}
