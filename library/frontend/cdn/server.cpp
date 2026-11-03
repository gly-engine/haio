#include <haio_cdn.hpp>
#include <haio_platform.hpp>
#include <haio/internal/cdn/clients.hpp>
#include <haio/stage.hpp>

#include <haio_url.hpp>

#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <numeric>
#include <string_view>

namespace {

using Haio::Platform::HttpServerRequest;
using Haio::Platform::HttpServerResponse;

struct Route {
    std::string query;
};

Route parseRoute(std::string_view target) {
    const auto parsed = target.starts_with('/') && !target.starts_with("//") ? Haio::Url::parse(target) : std::nullopt;
    if (!parsed || !parsed->scheme.empty()) throw std::runtime_error("invalid request target: " + std::string(target));

    if (parsed->path != "/cdn" && parsed->path != "/cdn/") {
        throw std::runtime_error("expected /cdn?<bucket>:<path>, as /cdn?assets:logo.png");
    }
    return Route{parsed->hasQuery ? parsed->query : std::string{}};
}

std::string downloadName(std::string_view path, Haio::Format format) {
    const auto slash = path.find_last_of('/');
    const auto base = slash == std::string_view::npos ? path : path.substr(slash + 1);

    std::string name;
    for (const char c : base) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.') name.push_back(c);
    }
    if (const auto dot = name.find_last_of('.'); dot != std::string::npos) name.resize(dot);
    if (name.find_first_not_of('.') == std::string::npos) name = "download";

    if (format == Haio::Format::RAW) return name;
    return name + '.' + std::string(Haio::extensionFor(format));
}

Haio::Format preferredFormat(const Haio::Parser::Parsed& parsed, const std::vector<Haio::Blob>& blobs) {
    for (const auto& step : parsed.steps) {
        if (step.kind == Haio::TokenKind::Generate) return Haio::Format::PNG;
        if (step.kind != Haio::TokenKind::Source) continue;
        const auto found = blobs.empty() ? Haio::Format::PNG : blobs.front().format;
        return found == Haio::Format::RAW || found == Haio::Format::PPM ? Haio::Format::PNG : found;
    }
    return Haio::Format::PNG;
}

bool nothingToDo(const Haio::Parser::Parsed& parsed, const std::vector<Haio::Blob>& blobs) {
    if (blobs.size() != 1 || parsed.outputColor) return false;

    const bool named = parsed.outputFormat != Haio::Format::RAW || !parsed.outputFormatName.empty();
    if (named && parsed.outputFormat != blobs.front().format) return false;

    return std::ranges::all_of(parsed.steps, [](const Haio::Token& step) {
        return step.kind == Haio::TokenKind::Source || step.kind == Haio::TokenKind::Decode;
    });
}

HttpServerResponse makeResponse(int status, std::string_view contentType, std::vector<uint8_t> body) {
    return HttpServerResponse{status, {{"content-type", std::string(contentType)}}, std::move(body)};
}

HttpServerResponse makeText(int status, std::string text) {
    return makeResponse(status, "text/plain; charset=utf-8", std::vector<uint8_t>(text.begin(), text.end()));
}

/** what went wrong decides the status, instead of everything collapsing into 400 */
constexpr int statusFor(Haio::ErrorCode code) {
    switch (code) {
        case Haio::ErrorCode::NotFound: return 404;
        case Haio::ErrorCode::UnsupportedFormat: return 415;
        case Haio::ErrorCode::Upstream: return 502;
        case Haio::ErrorCode::Timeout: return 504;
        case Haio::ErrorCode::InvalidInput: return 400;
        case Haio::ErrorCode::Io:
        case Haio::ErrorCode::Internal: return 500;
    }
    return 500;
}

HttpServerResponse makeError(const Haio::Error& error) {
    return makeText(statusFor(error.code), error.message + "\n");
}

enum class Encoding { Identity, Gzip, Deflate };

/**
 * the encoding the client prefers among the ones haio writes, read off accept-encoding
 * with its q values honoured.
 *
 * a tie goes to gzip, because every client that says deflate also says gzip and the
 * reverse is not true of old ones. "*" stands for gzip when gzip was not named on its
 * own, and a q of zero is a refusal, which is the one part of the header that is
 * easy to get backwards.
 */
Encoding negotiateEncoding(std::string_view header) {
    double gzip = -1;
    double deflate = -1;
    double any = -1;

    const auto trim = [](std::string_view text) {
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
        while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
        return text;
    };
    const auto named = [](std::string_view text, std::string_view name) {
        return std::ranges::equal(text, name, [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == b;
        });
    };

    while (!header.empty()) {
        const auto comma = header.find(',');
        auto item = header.substr(0, comma);
        header = comma == std::string_view::npos ? std::string_view{} : header.substr(comma + 1);

        double q = 1;
        if (const auto semicolon = item.find(';'); semicolon != std::string_view::npos) {
            const auto param = trim(item.substr(semicolon + 1));
            if (param.size() > 2 && (param[0] == 'q' || param[0] == 'Q') && param[1] == '=') {
                q = std::strtod(std::string(param.substr(2)).c_str(), nullptr);
            }
            item = item.substr(0, semicolon);
        }
        item = trim(item);

        if (named(item, "gzip") || named(item, "x-gzip")) gzip = q;
        else if (named(item, "deflate")) deflate = q;
        else if (item == "*") any = q;
    }

    if (gzip < 0) gzip = any;
    if (deflate < 0) deflate = any;
    if (gzip <= 0 && deflate <= 0) return Encoding::Identity;
    return gzip >= deflate ? Encoding::Gzip : Encoding::Deflate;
}

/**
 * a png, a jpeg and a gif are compressed already, and deflating them again spends the
 * cpu to hand back a body a few bytes larger. everything else haio writes is pixels
 * laid out plainly, and a ppm or a tga shrinks the way any bitmap does.
 */
bool worthCompressing(std::string_view contentType) {
    return contentType != "image/png" && contentType != "image/jpeg" && contentType != "image/gif";
}

/**
 * the body in the encoding negotiated, in one deflate call over a buffer sized for the
 * worst case. "deflate" in http is the zlib format and not raw deflate, whatever the
 * name suggests, and gzip is the same stream with a different header on it.
 */
Haio::Result<std::vector<uint8_t>> compressBody(const std::vector<uint8_t>& body, Encoding encoding) {
    z_stream stream{};
    const int window = encoding == Encoding::Gzip ? 15 + 16 : 15;
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, window, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        HAIO_FAIL(Internal, "cannot start the compressor");
    }

    // left uninitialised: the bound is larger than the input and only the front of it
    // is ever written, so zeroing it would be the most expensive part of a small body
    const auto capacity = deflateBound(&stream, static_cast<uLong>(body.size()));
    const auto out = std::make_unique_for_overwrite<uint8_t[]>(capacity);

    stream.next_in = const_cast<Bytef*>(body.data());
    stream.avail_in = static_cast<uInt>(body.size());
    stream.next_out = out.get();
    stream.avail_out = static_cast<uInt>(capacity);

    const int result = deflate(&stream, Z_FINISH);
    const auto written = stream.total_out;
    deflateEnd(&stream);
    if (result != Z_STREAM_END) HAIO_FAIL(Internal, "the response could not be compressed");

    return std::vector<uint8_t>(out.get(), out.get() + written);
}

/**
 * requests one address may make per second, as a leaky bucket, the way nginx meters
 * with limit_req.
 *
 * an address holds up to one second of credit and spends one per request, so a client
 * that has been quiet may burst that much at once and is then held to the rate. a
 * fixed window would instead let twice the rate through across a boundary, and would
 * refuse a client that had simply been idle.
 */
class RateLimiter {
public:
    explicit RateLimiter(size_t perSecond) : rate_(static_cast<double>(perSecond)) {}

    bool allow(const std::string& client) {
        if (rate_ <= 0 || client.empty()) return true;

        const auto now = std::chrono::steady_clock::now();

        // a bucket that has refilled says nothing a new one would not
        auto& bucket = seen_.row(client, Bucket{rate_, now}, [&](const Bucket& old) { return credit(old, now) >= rate_; });
        bucket.tokens = credit(bucket, now);
        bucket.filled = now;

        if (bucket.tokens < 1.0) return false;
        bucket.tokens -= 1.0;
        return true;
    }

private:
    struct Bucket {
        double tokens = 0;
        std::chrono::steady_clock::time_point filled;
    };

    /** what the bucket holds now, refilled at the rate and never above one second of it */
    double credit(const Bucket& bucket, std::chrono::steady_clock::time_point now) const {
        const auto idle = std::chrono::duration<double>(now - bucket.filled).count();
        return std::min(rate_, bucket.tokens + idle * rate_);
    }

    double rate_;
    Haio::Cdn::Detail::ClientTable<Bucket> seen_;
};

/**
 * a resize allocates width times height times four bytes before anything checks that
 * the number is sane, so "?resize=99999x99999" is a memory bomb written in a url. the
 * cap is on each side rather than on the area, the way a gpu states a texture limit.
 */
std::optional<Haio::Error> checkSize(const Haio::Cdn::Config& config, const std::vector<Haio::Token>& tokens) {
    const auto& limits = config.security;

    for (const auto& token : tokens) {
        if (token.kind != Haio::TokenKind::Resize) continue;

        // a share is checked as a share: the factor is known here even though what it
        // comes to is not, and bounding the factor bounds the result
        if (token.percent != 0) {
            if (limits.maxSizePercent > 0 && token.percent > limits.maxSizePercent) {
                return Haio::Error{Haio::ErrorCode::InvalidInput,
                                   "resize is limited to " + std::to_string(limits.maxSizePercent) + " percent"};
            }
            continue;
        }

        if (limits.maxSizePixel > 0
            && (token.size.width > limits.maxSizePixel || token.size.height > limits.maxSizePixel)) {
            return Haio::Error{Haio::ErrorCode::InvalidInput,
                               "resize is limited to " + std::to_string(limits.maxSizePixel) + " on each side"};
        }
    }
    return std::nullopt;
}

/** the fetch and the conversion behind it, which is exactly what an entry saves */
Haio::Task<Haio::Result<Haio::Cdn::CacheEntry>> produce(const Haio::Cdn::Config& config, Haio::Source::Archives& archives, Route route) {
    const auto words = Haio::Cdn::wordsOf(route.query);
    if (words.empty()) {
        co_return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput,
                                              "expected /cdn?<bucket>:<path>, as /cdn?assets:logo.png"});
    }

    const auto parsed = Haio::Parser::parse(words, {.bucketPrefix = true, .outputNamed = "the answer"});
    if (parsed.error) co_return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput, parsed.error.message});

    if (parsed.inputs.size() > config.security.maxSources) {
        co_return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput,
                                              "a request may name " + std::to_string(config.security.maxSources)
                                                  + " sources and this one names " + std::to_string(parsed.inputs.size())});
    }

    if (auto tooBig = checkSize(config, parsed.steps)) co_return std::unexpected(*tooBig);

    std::vector<Haio::Blob> blobs;
    blobs.reserve(parsed.inputs.size());
    for (const auto& input : parsed.inputs) {
        auto fetched = co_await Haio::Cdn::fetchBucket(config, archives, input.bucket, input.path);
        if (!fetched) co_return std::unexpected(fetched.error());

        if (fetched->format == Haio::Format::RAW) {
            co_return std::unexpected(Haio::Error{Haio::ErrorCode::UnsupportedFormat,
                                                  "haio does not recognise " + input.path});
        }
        blobs.push_back(*std::move(fetched));
    }

    Haio::Pipeline pipeline;
    if (!nothingToDo(parsed, blobs)) {
        for (const auto& step : parsed.steps) pipeline |= step;
    }

    if (!pipeline.tokens().empty()) {
        const bool named = parsed.outputFormat != Haio::Format::RAW || !parsed.outputFormatName.empty();
        const auto format = named          ? parsed.outputFormat
                          : parsed.outputColor ? Haio::Format::RAW
                                               : preferredFormat(parsed, blobs);
        pipeline |= Haio::Tokens::Encode(format, parsed.outputColor, parsed.outputSettings);
    }

    if (parsed.outputEncoding != Haio::BodyEncoding::Identity || !parsed.outputName.empty()) {
        pipeline |= Haio::Tokens::Deliver(parsed.outputEncoding, parsed.outputName);
    }

    const auto deadline = std::chrono::steady_clock::now() + config.security.timeout;
    auto converted = Haio::runPipeline(std::move(blobs), pipeline, deadline);
    if (!converted) co_return std::unexpected(converted.error());

    co_return Haio::Cdn::CacheEntry{
        std::move(converted->data),
        converted->contentType.empty() ? std::string(Haio::contentTypeFor(converted->format)) : converted->contentType,
        downloadName(converted->path, converted->format),
        parsed.outputEncoding,
    };
}

/** what every request shares, made once when the server starts */
struct Shared {
    Haio::Cdn::Config config;
    // one cache for the whole server, or it would cache nothing for anybody
    Haio::Cdn::Cache cache;
    // one limiter, because a limit counted per connection is no limit
    RateLimiter limiter;
    // one set of archives: one read for one request is there for the next one
    std::shared_ptr<Haio::Source::Archives> archives;
};

Haio::Task<HttpServerResponse> handleRequest(std::shared_ptr<Shared> shared, HttpServerRequest req) {
    if (req.method != "GET" && req.method != "HEAD") {
        co_return makeText(405, "method not allowed\n");
    }

    // counted before anything is read or fetched, so a flood costs as little as possible
    if (!shared->limiter.allow(req.client)) {
        auto res = makeText(429, "too many requests\n");
        res.headers.emplace_back("retry-after", "1");
        co_return res;
    }

    Route route;
    try {
        route = parseRoute(req.target);
    } catch (const std::exception& err) {
        // only the request parsing can land here, and its messages are ours
        co_return makeText(400, std::string(err.what()) + "\n");
    }

    // the key leaves the method out on purpose, so a HEAD finds what a GET stored
    const auto key = Haio::Cdn::cacheKey(route.query);
    auto entry = co_await shared->cache.fetch(key, req.client, [shared, route]() {
        return produce(shared->config, *shared->archives, route);
    });
    if (!entry) co_return makeError(entry.error());

    /**
     * compressed after the cache and not before it, so one entry answers every
     * client whatever it accepts. vary goes on everything that could have been
     * compressed, including what was not, or a proxy in between would hand the
     * gzip it stored for one client to another that never asked for it.
     */
    auto body = std::move(entry->data);
    const auto bodyEncoding = entry->encoding;
    const bool compressible = worthCompressing(entry->contentType)
                           && !body.empty() && body.size() <= std::numeric_limits<uInt>::max();
    const auto encoding = compressible ? negotiateEncoding(req.header("accept-encoding")) : Encoding::Identity;

    if (encoding != Encoding::Identity) {
        auto compressed = compressBody(body, encoding);
        if (!compressed) co_return makeError(compressed.error());
        body = *std::move(compressed);
    }

    auto res = makeResponse(200, entry->contentType, std::move(body));
    res.headers.emplace_back("content-disposition", "inline; filename=\"" + entry->filename + "\"");
    if (compressible) res.headers.emplace_back("vary", "Accept-Encoding");
    if (encoding == Encoding::Gzip) res.headers.emplace_back("content-encoding", "gzip");
    if (encoding == Encoding::Deflate) res.headers.emplace_back("content-encoding", "deflate");

    if (bodyEncoding != Haio::BodyEncoding::Identity) {
        res.headers.emplace_back("x-encoding", std::string(Haio::Stages::enumName(bodyEncoding)));
    }
    co_return res;
}

/**
 * these are what stands between the server and somebody deciding to spend all of its
 * memory, so leaving one out is worth saying out loud. each has a default and the
 * server is safe without them: the warning is there so nobody believes they
 * configured a limit they never wrote.
 */
void warnAboutDefaults(const Haio::Cdn::Config& config, bool cacheEnabled) {
    for (const auto& [name, bucket] : config.buckets) {
        if (bucket.open) {
            std::cerr << "warning: bucket \"" << name << "\" is open, so it will fetch whatever host the request names\n";
        }
    }

    const auto missing = [&config](std::string_view key) { return !config.declared.contains(std::string(key)); };

    if (missing("security.timeout")) {
        std::cerr << "warning: security.timeout is not set, so a conversion is stopped after "
                  << config.security.timeout.count() << "s by default\n";
    }
    if (missing("security.max_size_pixel")) {
        std::cerr << "warning: security.max_size_pixel is not set, so a resize is capped at "
                  << config.security.maxSizePixel << " on each side by default\n";
    }
    if (missing("security.max_size_percent")) {
        std::cerr << "warning: security.max_size_percent is not set, so a resize is capped at "
                  << config.security.maxSizePercent << " percent by default\n";
    }
    if (missing("security.max_sources")) {
        std::cerr << "warning: security.max_sources is not set, so one request may ask the server to fetch "
                  << config.security.maxSources << " files by default\n";
    }
    if (missing("security.max_requests_by_ip")) {
        // no default: a number picked blind would shut out a whole office behind one
        // address, the same reason max_cache_entries_by_ip has none
        std::cerr << "warning: security.max_requests_by_ip is not set, so one address may "
                     "make as many requests per second as it likes\n";
    }
    if (cacheEnabled && missing("cache.max_usage")) {
        std::cerr << "warning: cache.max_usage is not set, so the cache is capped at "
                  << config.cache.maxUsage / (1024 * 1024) << "mb by default\n";
    }
    if (cacheEnabled && missing("security.max_cache_entries_by_ip")) {
        std::cerr << "warning: security.max_cache_entries_by_ip is not set, so one address "
                     "may fill the cache on its own and evict everybody else\n";
    }
}

}

namespace Haio::Cdn {

Task<void> runServer(Config config) {
    auto shared = std::make_shared<Shared>(Shared{
        config,
        Cache{config.cache, config.security.maxCacheEntriesByIp},
        RateLimiter{config.security.maxRequestsByIp},
        Source::makeArchives(config.security.maxUnzip),
    });

    if (shared->cache.enabled()) {
        std::cout << "cache: " << (config.cache.url.empty() ? "memory" : config.cache.url)
                  << ", ttl " << config.cache.ttl.count() << "s\n";
    }
    warnAboutDefaults(config, shared->cache.enabled());

    Platform::HttpServerOptions options;
    options.host = config.host;
    options.port = config.port;
    options.name = "haio-cdn";
    options.ready = [host = config.host, port = config.port] {
        std::cout << "haio cdn listening on http://" << host << ':' << port << "\n";
    };

    co_await Platform::serveHttp(std::move(options), [shared](HttpServerRequest req) {
        return handleRequest(shared, std::move(req));
    });
}

}
