#include <haio_cdn.hpp>

#include <internal/bucket.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/url/parse.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <numeric>
#include <string_view>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

namespace {

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

    const auto encodedPath = std::string(parsed->encoded_path());
    constexpr std::string_view prefix = "/cdn/";
    if (!encodedPath.starts_with(prefix)) throw std::runtime_error("expected /cdn/<bucket>/<path>");

    std::vector<std::string> segments;
    for (auto segment : parsed->segments()) segments.emplace_back(segment);
    if (segments.empty() || segments.front() != "cdn") throw std::runtime_error("expected /cdn/<bucket>/<path>");

    const auto tail = std::string_view(encodedPath).substr(prefix.size());
    const auto query = parsed->has_query() ? std::string(parsed->encoded_query()) : std::string{};
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
    return name + '.' + std::string(Haio::extensionFor(format));
}

bool hasTokenKind(const std::vector<Haio::Token>& tokens, Haio::TokenKind kind) {
    return std::ranges::any_of(tokens, [kind](const Haio::Token& token) { return token.kind == kind; });
}

bool hasImageTransform(const std::vector<Haio::Token>& tokens) {
    return std::ranges::any_of(tokens, [](const Haio::Token& token) {
        return token.kind == Haio::TokenKind::Crop || token.kind == Haio::TokenKind::Resize || token.kind == Haio::TokenKind::Radius;
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
bool alreadyWhatWasAsked(const std::vector<Haio::Token>& tokens, Haio::Format format) {
    if (tokens.empty() || format == Haio::Format::RAW) return false;

    return std::ranges::all_of(tokens, [format](const Haio::Token& token) {
        return token.kind == Haio::TokenKind::Encode && token.format == format && !token.color;
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

/** what went wrong decides the status, instead of everything collapsing into 400 */
constexpr http::status statusFor(Haio::ErrorCode code) {
    switch (code) {
        case Haio::ErrorCode::NotFound: return http::status::not_found;
        case Haio::ErrorCode::UnsupportedFormat: return http::status::unsupported_media_type;
        case Haio::ErrorCode::Upstream: return http::status::bad_gateway;
        case Haio::ErrorCode::Timeout: return http::status::gateway_timeout;
        case Haio::ErrorCode::InvalidInput: return http::status::bad_request;
        case Haio::ErrorCode::Io:
        case Haio::ErrorCode::Internal: return http::status::internal_server_error;
    }
    return http::status::internal_server_error;
}

http::response<http::vector_body<uint8_t>> makeError(const Haio::Error& error) {
    return makeText(statusFor(error.code), error.message + "\n");
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

        /**
         * a bucket that has refilled says nothing a new one would not, so those are
         * the ones dropped when the table grows. a caller with many addresses must
         * not be able to make this table the thing that runs out.
         */
        if (seen_.size() > clientsBeforePrune) {
            std::erase_if(seen_, [&](const auto& row) { return credit(row.second, now) >= rate_; });
            if (seen_.size() > clientsBeforePrune) seen_.clear();
        }

        auto found = seen_.find(client);
        if (found == seen_.end()) found = seen_.emplace(client, Bucket{rate_, now}).first;

        auto& bucket = found->second;
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

    static constexpr size_t clientsBeforePrune = 10000;
    double rate_;
    std::map<std::string, Bucket> seen_;
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
asio::awaitable<Haio::Result<Haio::Cdn::CacheEntry>> produce(const Haio::Cdn::Config& config, Haio::Cdn::Bucket::ZipArchives& archives, Route route) {
    auto fetched = co_await Haio::Cdn::fetchBucket(config, archives, route.bucket, route.path);
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
        co_return std::unexpected(Haio::Error{Haio::ErrorCode::UnsupportedFormat,
                                              "haio does not recognise " + route.path});
    }

    auto tokens = Haio::parseQueryTokens(route.query);
    if (auto tooBig = checkSize(config, tokens)) co_return std::unexpected(*tooBig);

    if (!tokens.empty() && !alreadyWhatWasAsked(tokens, blob.format)) {
        Haio::Pipeline pipeline;
        pipeline |= Haio::Tokens::Source(route.bucket, route.path);
        for (auto token : tokens) pipeline |= std::move(token);

        if (hasImageTransform(pipeline.tokens()) && !hasTokenKind(pipeline.tokens(), Haio::TokenKind::Encode)) {
            const auto fallback = blob.format == Haio::Format::RAW ? Haio::Format::PNG : blob.format;
            pipeline |= Haio::Tokens::Encode(fallback == Haio::Format::PPM ? Haio::Format::PNG : fallback);
        }

        auto converted = Haio::runPipeline(std::move(blob), pipeline);
        if (!converted) co_return std::unexpected(converted.error());
        blob = *std::move(converted);
    }

    co_return Haio::Cdn::CacheEntry{
        std::move(blob.data),
        blob.contentType.empty() ? std::string(Haio::contentTypeFor(blob.format)) : blob.contentType,
        downloadName(route.path, blob.format),
    };
}

asio::awaitable<http::response<http::vector_body<uint8_t>>> handleRequest(const Haio::Cdn::Config& config, Haio::Cdn::Cache cache, std::shared_ptr<RateLimiter> limiter, std::shared_ptr<Haio::Cdn::Bucket::ZipArchives> archives, std::string client, http::request<http::string_body> req) {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        co_return makeText(http::status::method_not_allowed, "method not allowed\n");
    }

    // counted before anything is read or fetched, so a flood costs as little as possible
    if (!limiter->allow(client)) {
        auto res = makeText(http::status::too_many_requests, "too many requests\n");
        res.set(http::field::retry_after, "1");
        co_return res;
    }

    try {
        const auto route = parseRoute(req.target());

        // the key leaves the method out on purpose, so a HEAD finds what a GET stored
        const auto key = Haio::Cdn::cacheKey(route.bucket, route.path, route.query);
        auto entry = co_await cache.fetch(key, client, [&config, archives, route]() -> asio::awaitable<Haio::Result<Haio::Cdn::CacheEntry>> {
            return produce(config, *archives, route);
        });
        if (!entry) co_return makeError(entry.error());

        auto res = makeResponse(http::status::ok, entry->contentType, std::move(entry->data));
        res.set(http::field::content_disposition, "inline; filename=\"" + entry->filename + "\"");
        co_return res;
    } catch (const std::exception& err) {
        // only the request parsing above can land here now, and its messages are ours
        co_return makeText(http::status::bad_request, std::string(err.what()) + "\n");
    }
}

asio::awaitable<void> session(tcp::socket socket, Haio::Cdn::Config config, Haio::Cdn::Cache cache, std::shared_ptr<RateLimiter> limiter, std::shared_ptr<Haio::Cdn::Bucket::ZipArchives> archives) {
    /**
     * the address is read once, before anything can close the socket under us.
     *
     * @todo behind a proxy every request arrives from the proxy, so the per address
     * quota would see one caller. honouring x-forwarded-for needs a list of proxies
     * worth believing, since the header is written by whoever sent the request.
     */
    std::string client;
    if (boost::system::error_code ec; true) {
        const auto peer = socket.remote_endpoint(ec);
        if (!ec) client = peer.address().to_string();
    }

    beast::flat_buffer buffer;
    try {
        for (;;) {
            http::request<http::string_body> req;
            co_await http::async_read(socket, buffer, req, asio::use_awaitable);
            const bool close = req.need_eof();
            const bool headOnly = req.method() == http::verb::head;
            auto res = co_await handleRequest(config, cache, limiter, archives, client, std::move(req));
            if (headOnly) {
                const auto size = res.body().size();
                res.body().clear();
                res.content_length(size);
            }
            res.keep_alive(!close);
            co_await http::async_write(socket, res, asio::use_awaitable);
            if (close) break;
        }
    } catch (const boost::system::system_error& err) {
        // a client closing a keep alive connection arrives here as end_of_stream, and
        // it is the ordinary way a request ends rather than something to report. every
        // other reason is logged, because a swallowed error leaves nothing to debug
        if (err.code() != http::error::end_of_stream) {
            std::cerr << "connection dropped: " << err.code().message() << "\n";
        }
    } catch (const std::exception& err) {
        std::cerr << "connection dropped: " << err.what() << "\n";
    }

    beast::error_code ec;
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

asio::awaitable<void> listener(Haio::Cdn::Config config) {
    auto executor = co_await asio::this_coro::executor;
    tcp::resolver resolver(executor);
    const auto resolved = co_await resolver.async_resolve(config.host, std::to_string(config.port), asio::use_awaitable);
    tcp::acceptor acceptor(executor, *resolved.begin());

    for (const auto& [name, bucket] : config.buckets) {
        if (bucket.open) {
            std::cerr << "warning: bucket \"" << name << "\" is open, so it will fetch whatever host the request names\n";
        }
    }

    // one cache for the whole server: a Cache holds a shared impl, so copying into a
    // session shares the store and the in flight map rather than making new ones
    Haio::Cdn::Cache cache{config.cache, config.security.maxCacheEntriesByIp, executor};
    // shared by every session, because a limit counted per connection is no limit
    auto limiter = std::make_shared<RateLimiter>(config.security.maxRequestsByIp);
    // shared too: an archive read for one request is there for the next one
    auto archives = std::make_shared<Haio::Cdn::Bucket::ZipArchives>(config.security.maxUnzip);
    if (cache.enabled()) {
        std::cout << "cache: " << (config.cache.url.empty() ? "memory" : config.cache.url)
                  << ", ttl " << config.cache.ttl.count() << "s\n";
    }

    /**
     * these three are what stands between the server and somebody deciding to spend
     * all of its memory, so leaving one out is worth saying out loud. each has a
     * default and the server is safe without them: the warning is there so nobody
     * believes they configured a limit they never wrote.
     */
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
    if (cache.enabled() && missing("cache.max_usage")) {
        std::cerr << "warning: cache.max_usage is not set, so the cache is capped at "
                  << config.cache.maxUsage / (1024 * 1024) << "mb by default\n";
    }
    if (cache.enabled() && missing("security.max_cache_entries_by_ip")) {
        std::cerr << "warning: security.max_cache_entries_by_ip is not set, so one address "
                     "may fill the cache on its own and evict everybody else\n";
    }
    std::cout << "haio cdn listening on http://" << config.host << ':' << config.port << "\n";

    for (;;) {
        auto socket = co_await acceptor.async_accept(asio::use_awaitable);
        asio::co_spawn(executor, session(std::move(socket), config, cache, limiter, archives), asio::detached);
    }
}

}

namespace Haio::Cdn {

asio::awaitable<void> runServer(Config config) {
    co_await listener(std::move(config));
}

}
