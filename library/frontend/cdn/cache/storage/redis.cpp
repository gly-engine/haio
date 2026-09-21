#include <haio_cache.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/redis/connection.hpp>

// boost.redis is not header only, and this is the one translation unit that compiles
// it: src.hpp exists for exactly that, and keeping it here means the library comes
// and goes with the file that uses it
#include <boost/redis/src.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace asio = boost::asio;
namespace redis = boost::redis;
namespace urls = boost::urls;

namespace {

/**
 * a shared redis, so several haio processes answer from one cache.
 *
 * every call goes through asTuple: a cache that cannot be reached has to read as a
 * miss and let the request carry on to the upstream, never as a failed response. the
 * whole point of a cache is that losing it costs latency and nothing else.
 */
class RedisStore final : public Haio::Cdn::CacheStore {
public:
    RedisStore(const std::string& url, std::chrono::seconds ttl, size_t maxUsage, asio::any_io_executor executor)
        : ttl_(ttl), maxUsage_(maxUsage), connection_(std::make_shared<redis::connection>(executor)) {
        const auto parsed = urls::parse_uri(url);
        if (!parsed) throw std::runtime_error("redis cache url is not a url: " + url);

        redis::config config;
        if (!parsed->host().empty()) config.addr.host = std::string(parsed->host());
        if (parsed->has_port()) config.addr.port = std::string(parsed->port());
        if (!parsed->userinfo().empty()) {
            config.username = std::string(parsed->user());
            config.password = std::string(parsed->password());
        }
        // redis://host:port/3 selects database 3, the way every other client reads it
        if (const auto path = std::string(parsed->path()); path.size() > 1) {
            config.database_index = std::stoi(path.substr(1));
        }
        config.use_ssl = parsed->scheme() == "rediss";

        connection_->async_run(config, asio::consign(asio::detached, connection_));
    }

    ~RedisStore() override { connection_->cancel(); }

    asio::awaitable<std::optional<Haio::Cdn::CacheEntry>> get(const std::string& key) override {
        // read, renew and touch the index in one round trip rather than three
        redis::request request;
        request.push("GET", prefixed(key));
        request.push("EXPIRE", prefixed(key), std::to_string(ttl_.count()), "XX");
        request.push("ZADD", index(), std::to_string(stamp()), key);

        redis::response<std::optional<std::string>, redis::ignore_t, redis::ignore_t> reply;
        const auto [error, size] = co_await connection_->async_exec(request, reply, asio::as_tuple(asio::use_awaitable));
        if (error) {
            warnOnce(error.message());
            co_return std::nullopt;
        }

        const auto& value = std::get<0>(reply);
        if (!value.has_value() || !value->has_value()) co_return std::nullopt;

        const auto& raw = **value;
        co_return Haio::Cdn::decodeEntry({reinterpret_cast<const uint8_t*>(raw.data()), raw.size()});
    }

    asio::awaitable<void> put(const std::string& key, const Haio::Cdn::CacheEntry& entry) override {
        const auto raw = Haio::Cdn::encodeEntry(entry);

        redis::request request;
        // redis owns the expiry, so an entry cannot outlive the ttl even across restarts
        request.push("SET", prefixed(key), std::string_view(reinterpret_cast<const char*>(raw.data()), raw.size()),
                     "EX", std::to_string(ttl_.count()));
        request.push("ZADD", index(), std::to_string(stamp()), key);
        request.push("HSET", sizes(), key, std::to_string(raw.size()));
        request.push("EXPIRE", index(), std::to_string(ttl_.count() * 2));
        request.push("EXPIRE", sizes(), std::to_string(ttl_.count() * 2));

        redis::response<redis::ignore_t, redis::ignore_t, redis::ignore_t,
                        redis::ignore_t, redis::ignore_t> reply;
        const auto [error, size] = co_await connection_->async_exec(request, reply, asio::as_tuple(asio::use_awaitable));
        if (error) {
            warnOnce(error.message());
            co_return;
        }

        if (++sinceTrim_ >= trimEvery) {
            sinceTrim_ = 0;
            co_await trim();
        }
        co_return;
    }

private:
    /** the keys are namespaced so a shared redis can hold more than haio */
    static std::string prefixed(const std::string& key) { return "haio:" + key; }

    /** a sorted set of the keys haio owns, scored by when each was last touched */
    static std::string index() { return "haio:index"; }

    static long long stamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /** what each key costs, so the budget can be met without asking redis to measure */
    static std::string sizes() { return "haio:sizes"; }

    /**
     * redis expires entries on its own, but nothing would stop haio from asking it to
     * hold a gigabyte first. the index says which keys are ours and when each was last
     * wanted; the hash beside it says what each one costs.
     *
     * the two have to be reconciled rather than trusted: redis drops a key when its
     * ttl runs out and tells nobody, so an index left alone would keep counting bytes
     * that are already gone and evict entries that are still wanted. every pass checks
     * which keys are really there and forgets the rest.
     *
     * this is about being a decent tenant of a shared redis rather than about
     * correctness: redis has its own maxmemory, and that one is not ours to set.
     */
    asio::awaitable<void> trim() {
        redis::request listing;
        listing.push("ZRANGE", index(), "0", "-1");

        redis::response<std::vector<std::string>> listed;
        if (const auto [error, size] = co_await connection_->async_exec(listing, listed, asio::as_tuple(asio::use_awaitable)); error) {
            warnOnce(error.message());
            co_return;
        }

        const auto& keys = std::get<0>(listed).value();
        if (keys.empty()) co_return;

        // oldest first, and ask in one round trip what is still there and what it costs
        redis::request measuring;
        for (const auto& key : keys) measuring.push("EXISTS", prefixed(key));

        redis::generic_response present;
        if (const auto [error, size] = co_await connection_->async_exec(measuring, present, asio::as_tuple(asio::use_awaitable)); error) {
            warnOnce(error.message());
            co_return;
        }

        redis::request weighing;
        for (const auto& key : keys) weighing.push("HGET", sizes(), key);

        redis::generic_response weights;
        if (const auto [error, size] = co_await connection_->async_exec(weighing, weights, asio::as_tuple(asio::use_awaitable)); error) {
            warnOnce(error.message());
            co_return;
        }

        std::vector<size_t> cost(keys.size(), 0);
        std::vector<bool> alive(keys.size(), false);
        for (size_t at = 0; at < keys.size(); at++) {
            if (at < present.value().size()) alive[at] = present.value()[at].value == "1";
            if (at < weights.value().size()) {
                const auto& text = weights.value()[at].value;
                if (!text.empty()) cost[at] = static_cast<size_t>(std::strtoull(text.c_str(), nullptr, 10));
            }
        }

        size_t held = 0;
        for (size_t at = 0; at < keys.size(); at++) {
            if (alive[at]) held += cost[at];
        }

        redis::request dropping;
        bool dropped = false;

        // whatever redis already expired is forgotten here, not counted against anyone
        for (size_t at = 0; at < keys.size(); at++) {
            if (alive[at]) continue;
            dropping.push("ZREM", index(), keys[at]);
            dropping.push("HDEL", sizes(), keys[at]);
            dropped = true;
        }

        // then the least recently wanted, until what is left fits the budget
        for (size_t at = 0; at < keys.size() && held > maxUsage_; at++) {
            if (!alive[at]) continue;
            dropping.push("DEL", prefixed(keys[at]));
            dropping.push("ZREM", index(), keys[at]);
            dropping.push("HDEL", sizes(), keys[at]);
            held -= std::min(held, cost[at]);
            dropped = true;
        }

        if (!dropped) co_return;

        redis::generic_response ignored;
        if (const auto [error, size] = co_await connection_->async_exec(dropping, ignored, asio::as_tuple(asio::use_awaitable)); error) {
            warnOnce(error.message());
        }
        co_return;
    }

    // trimming on every write would double the round trips for no benefit
    static constexpr int trimEvery = 64;

    void warnOnce(const std::string& reason) {
        if (std::exchange(warned_, true)) return;
        std::cerr << "warning: redis cache unavailable, serving from upstream: " << reason << "\n";
    }

    std::chrono::seconds ttl_;
    size_t maxUsage_;
    int sinceTrim_ = 0;
    std::shared_ptr<redis::connection> connection_;
    bool warned_ = false;
};

}

namespace Haio::Cdn {

std::unique_ptr<CacheStore> makeRedisStore(std::string url, std::chrono::seconds ttl, size_t maxUsage, asio::any_io_executor executor) {
    return std::make_unique<RedisStore>(url, ttl, maxUsage, std::move(executor));
}

}
