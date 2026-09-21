#include <cache/cache.hpp>

#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <stdexcept>

namespace asio = boost::asio;
namespace urls = boost::urls;

namespace Haio::Cdn {

struct Cache::Impl {
    Impl(asio::any_io_executor executor, size_t maxEntriesByIp, std::chrono::seconds window)
        : flight(std::move(executor)), quota(maxEntriesByIp, window) {}

    std::unique_ptr<CacheStore> store;
    Detail::SingleFlight flight;
    Detail::ClientQuota quota;
};

namespace {

/** the scheme of the url picks the backend, the way it picks a bucket's reader */
std::unique_ptr<CacheStore> storeFor(const CacheConfig& config, asio::any_io_executor executor) {
    if (config.url.empty()) return makeMemoryStore(config.maxUsage, config.ttl);

    const auto parsed = urls::parse_uri(config.url);
    if (!parsed) throw std::runtime_error("cache url is not a url: " + config.url);

    const std::string scheme{parsed->scheme()};
    if (scheme == "file") {
        const std::string host{parsed->host()};
        const std::string path{parsed->path()};
        if (host.empty() && (path.empty() || path == "/")) {
            throw std::runtime_error("cache url has no directory: " + config.url);
        }
        return makeFileStore(std::filesystem::path(host + path), config.ttl, config.maxUsage);
    }
    if (scheme == "redis" || scheme == "rediss") {
        return makeRedisStore(config.url, config.ttl, config.maxUsage, std::move(executor));
    }
    // the config already rejected anything else, so reaching here is a bug not a typo
    throw std::runtime_error("unsupported cache scheme \"" + scheme + "\" in " + config.url);
}

}

Cache::Cache(CacheConfig config, size_t maxEntriesByIp, boost::asio::any_io_executor executor) {
    impl_ = std::make_shared<Impl>(executor, maxEntriesByIp, config.ttl);
    // a ttl of zero keeps nothing, and still deduplicates
    if (config.ttl.count() > 0) impl_->store = storeFor(config, std::move(executor));
}

bool Cache::enabled() const {
    return impl_ && impl_->store != nullptr;
}

asio::awaitable<Result<CacheEntry>> Cache::fetch(std::string key, std::string client, Producer produce) {
    if (!impl_) co_return co_await produce();

    if (impl_->store) {
        if (auto hit = co_await impl_->store->get(key)) co_return *std::move(hit);
    }

    // one producer for the key, whether or not what it makes is kept afterwards
    co_return co_await impl_->flight.run(key, [&]() -> asio::awaitable<Result<CacheEntry>> {
        auto made = co_await produce();
        if (made && impl_->store && impl_->quota.mayStore(client)) {
            co_await impl_->store->put(key, *made);
        }
        co_return made;
    });
}

}
