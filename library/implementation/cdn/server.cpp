#include <haio/implementation/registry.hpp>
#include <haio/implementation/cdn.hpp>
#include <haio/implementation/language/query.hpp>
#include <haio/implementation/language/runner.hpp>
#include <haio/implementation/platform.hpp>
#include <haio/implementation/cdn/clients.hpp>

#include <haio/util/url.hpp>

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

using Haio::Implementation::Interfaces::HttpServerRequest;
using Haio::Implementation::Interfaces::HttpServerResponse;

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
    // an origin form target is a path and a query, and nothing that could name a host
    const auto parsed = target.starts_with('/') && !target.starts_with("//") ? Haio::Util::Url::parse(target) : std::nullopt;
    if (!parsed || !parsed->scheme.empty()) throw std::runtime_error("invalid request target: " + std::string(target));

    const auto& encodedPath = parsed->path;
    constexpr std::string_view prefix = "/cdn/";
    if (!encodedPath.starts_with(prefix)) throw std::runtime_error("expected /cdn/<bucket>/<path>");

    const auto segments = parsed->segments();
    if (segments.empty() || segments.front() != "cdn") throw std::runtime_error("expected /cdn/<bucket>/<path>");

    const auto tail = std::string_view(encodedPath).substr(prefix.size());
    const auto query = parsed->hasQuery ? parsed->query : std::string{};
    if (tail.find('/') == std::string_view::npos) {
        return Route{"file", segments.size() > 1 ? segments[1] : std::string{}, query};
    }

    auto bucket = segments.size() > 1 ? segments[1] : std::string{};
    auto rest = segments.size() > 2 ? joinSegments(segments, 2) : std::string{};
    if (bucket.empty()) bucket = "file";
    return Route{std::move(bucket), std::move(rest), query};
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
    return name + '.' + std::string(Haio::Implementation::extensionFor(format));
}

bool hasTokenKind(const std::vector<Haio::Implementation::Language::Token>& tokens, Haio::Implementation::Language::TokenKind kind) {
    return std::ranges::any_of(tokens, [kind](const Haio::Implementation::Language::Token& token) { return token.kind == kind; });
}

bool hasImageTransform(const std::vector<Haio::Implementation::Language::Token>& tokens) {
    return std::ranges::any_of(tokens, [](const Haio::Implementation::Language::Token& token) {
        return token.kind == Haio::Implementation::Language::TokenKind::Crop || token.kind == Haio::Implementation::Language::TokenKind::Resize || token.kind == Haio::Implementation::Language::TokenKind::Radius;
    });
}

/**
 * asking for the format a file already is, with nothing else to do to it, is asking
 * for the file.
 *
 * the guard is that nothing else may be in the query. a crop or a resize has to be
 * decoded and written back out even when the container does not change, so this only
 * applies when the encode is the whole request. where it does apply it spends no cpu
 * and, more to the point, hands back the original bytes rather than a re-encode that
 * merely has the same pixels.
 *
 * a named colour counts as something else to do. two tga files are both tga and one
 * of them is sixteen bits a pixel, so "the container it already is" stops being the
 * whole question the moment -pix_fmt asks the other half of it.
 */
bool alreadyWhatWasAsked(const std::vector<Haio::Implementation::Language::Token>& tokens, Haio::Format format) {
    if (tokens.empty() || format == Haio::Format::RAW) return false;

    return std::ranges::all_of(tokens, [format](const Haio::Implementation::Language::Token& token) {
        return token.kind == Haio::Implementation::Language::TokenKind::Encode && token.format == format && !token.color;
    });
}

HttpServerResponse makeResponse(int status, std::string_view contentType, std::vector<uint8_t> body) {
    return HttpServerResponse{status, {{"content-type", std::string(contentType)}}, std::move(body)};
}

HttpServerResponse makeText(int status, std::string text) {
    return makeResponse(status, "text/plain; charset=utf-8", std::vector<uint8_t>(text.begin(), text.end()));
}

/** what went wrong decides the status, instead of everything collapsing into 400 */
constexpr int statusFor(Haio::Implementation::ErrorCode code) {
    switch (code) {
        case Haio::Implementation::ErrorCode::NotFound: return 404;
        case Haio::Implementation::ErrorCode::UnsupportedFormat: return 415;
        case Haio::Implementation::ErrorCode::Upstream: return 502;
        case Haio::Implementation::ErrorCode::Timeout: return 504;
        case Haio::Implementation::ErrorCode::InvalidInput: return 400;
        case Haio::Implementation::ErrorCode::Io:
        case Haio::Implementation::ErrorCode::Internal: return 500;
    }
    return 500;
}

HttpServerResponse makeError(const Haio::Implementation::Error& error) {
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
Haio::Implementation::Result<std::vector<uint8_t>> compressBody(const std::vector<uint8_t>& body, Encoding encoding) {
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
    Haio::Implementation::Cdn::ClientTable<Bucket> seen_;
};

/**
 * a resize allocates width times height times four bytes before anything checks that
 * the number is sane, so "?resize=99999x99999" is a memory bomb written in a url. the
 * cap is on each side rather than on the area, the way a gpu states a texture limit.
 */
std::optional<Haio::Implementation::Error> checkSize(const Haio::Implementation::Cdn::Config& config, const std::vector<Haio::Implementation::Language::Token>& tokens) {
    const auto& limits = config.security;

    for (const auto& token : tokens) {
        if (token.kind != Haio::Implementation::Language::TokenKind::Resize) continue;

        // a share is checked as a share: the factor is known here even though what it
        // comes to is not, and bounding the factor bounds the result
        if (token.percent != 0) {
            if (limits.maxSizePercent > 0 && token.percent > limits.maxSizePercent) {
                return Haio::Implementation::Error{Haio::Implementation::ErrorCode::InvalidInput,
                                   "resize is limited to " + std::to_string(limits.maxSizePercent) + " percent"};
            }
            continue;
        }

        if (limits.maxSizePixel > 0
            && (token.size.width > limits.maxSizePixel || token.size.height > limits.maxSizePixel)) {
            return Haio::Implementation::Error{Haio::Implementation::ErrorCode::InvalidInput,
                               "resize is limited to " + std::to_string(limits.maxSizePixel) + " on each side"};
        }
    }
    return std::nullopt;
}

/** the fetch and the conversion behind it, which is exactly what an entry saves */
Haio::Implementation::Platform::Task<Haio::Implementation::Result<Haio::Implementation::Cdn::CacheEntry>> produce(const Haio::Implementation::Cdn::Config& config, Haio::Implementation::Source::Archives& archives, Route route) {
    auto fetched = co_await Haio::Implementation::Cdn::fetchBucket(config, archives, route.bucket, route.path);
    if (!fetched) co_return std::unexpected(fetched.error());
    auto blob = *std::move(fetched);

    /**
     * a bucket is a directory, and a directory holds whatever somebody put there. only
     * files haio can name are served: without this the cdn hands out anything the
     * bucket happens to contain, which is a file server with extra steps rather than
     * an image one.
     *
     * naming it is enough, though. a png asked for without a format is handed over
     * exactly as it was stored, because there is nothing to convert it to.
     */
    if (blob.format == Haio::Format::RAW) {
        co_return std::unexpected(Haio::Implementation::Error{Haio::Implementation::ErrorCode::UnsupportedFormat,
                                              "haio does not recognise " + route.path});
    }

    auto tokens = Haio::Implementation::LanguageParser::parseQueryTokens(route.query);
    if (auto tooBig = checkSize(config, tokens)) co_return std::unexpected(*tooBig);

    if (!tokens.empty() && !alreadyWhatWasAsked(tokens, blob.format)) {
        Haio::Implementation::Language::Pipeline pipeline;
        pipeline |= Haio::Implementation::LanguageToken::Source(route.bucket, route.path);
        for (auto token : tokens) pipeline |= std::move(token);

        if (hasImageTransform(pipeline.tokens()) && !hasTokenKind(pipeline.tokens(), Haio::Implementation::Language::TokenKind::Encode)) {
            const auto fallback = blob.format == Haio::Format::RAW ? Haio::Format::PNG : blob.format;
            pipeline |= Haio::Implementation::LanguageToken::Encode(fallback == Haio::Format::PPM ? Haio::Format::PNG : fallback);
        }

        auto converted = Haio::Implementation::Language::runPipeline(std::move(blob), pipeline);
        if (!converted) co_return std::unexpected(converted.error());
        blob = *std::move(converted);
    }

    co_return Haio::Implementation::Cdn::CacheEntry{
        std::move(blob.data),
        blob.contentType.empty() ? std::string(Haio::Implementation::contentTypeFor(blob.format)) : blob.contentType,
        downloadName(route.path, blob.format),
    };
}

/** what every request shares, made once when the server starts */
struct Shared {
    Haio::Implementation::Cdn::Config config;
    // one cache for the whole server, or it would cache nothing for anybody
    Haio::Implementation::Cdn::Cache cache;
    // one limiter, because a limit counted per connection is no limit
    RateLimiter limiter;
    // one set of archives: one read for one request is there for the next one
    std::shared_ptr<Haio::Implementation::Source::Archives> archives;
};

Haio::Implementation::Platform::Task<HttpServerResponse> handleRequest(std::shared_ptr<Shared> shared, HttpServerRequest req) {
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
    const auto key = Haio::Implementation::Cdn::cacheKey(route.bucket, route.path, route.query);
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
    co_return res;
}

/**
 * these are what stands between the server and somebody deciding to spend all of its
 * memory, so leaving one out is worth saying out loud. each has a default and the
 * server is safe without them: the warning is there so nobody believes they
 * configured a limit they never wrote.
 */
void warnAboutDefaults(const Haio::Implementation::Cdn::Config& config, bool cacheEnabled) {
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

namespace Haio::Implementation::Cdn {

Platform::Task<void> runServer(Config config) {
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

    Interfaces::HttpServerOptions options;
    options.host = config.host;
    options.port = config.port;
    options.name = "haio-cdn";
    options.ready = [host = config.host, port = config.port] {
        std::cout << "haio cdn listening on http://" << host << ':' << port << "\n";
    };

    co_await Interfaces::serveHttp(std::move(options), [shared](HttpServerRequest req) {
        return handleRequest(shared, std::move(req));
    });
}

}
