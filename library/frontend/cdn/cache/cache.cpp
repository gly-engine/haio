#include <haio/internal/cdn/cache.hpp>

#include <haio_url.hpp>

#include <stdexcept>


namespace Haio::Cdn {

struct Cache::Impl {
    Impl(size_t maxEntriesByIp, std::chrono::seconds window) : quota(maxEntriesByIp, window) {}

    std::unique_ptr<CacheStore> store;
    Detail::SingleFlight flight;
    Detail::ClientQuota quota;
};

namespace {

/** the scheme of the url picks the backend, the way it picks a bucket's reader */
std::unique_ptr<CacheStore> storeFor(const CacheConfig& config) {
    if (config.url.empty()) return makeMemoryStore(config.maxUsage, config.ttl);

    const auto parsed = Url::parse(config.url);
    if (!parsed || parsed->scheme.empty()) throw std::runtime_error("cache url is not a url: " + config.url);

    const auto& scheme = parsed->scheme;
    if (scheme == "file") {
        const auto host = parsed->hostName();
        const auto path = parsed->decodedPath();
        if (host.empty() && (path.empty() || path == "/")) {
            throw std::runtime_error("cache url has no directory: " + config.url);
        }
        return makeFileStore(std::filesystem::path(host + path), config.ttl, config.maxUsage);
    }
    if (scheme == "redis" || scheme == "rediss") {
        return makeRedisStore(config.url, config.ttl, config.maxUsage);
    }
    // the config already rejected anything else, so reaching here is a bug not a typo
    throw std::runtime_error("unsupported cache scheme \"" + scheme + "\" in " + config.url);
}

}

Cache::Cache(CacheConfig config, size_t maxEntriesByIp) {
    impl_ = std::make_shared<Impl>(maxEntriesByIp, config.ttl);
    // a ttl of zero keeps nothing, and still deduplicates
    if (config.ttl.count() > 0) impl_->store = storeFor(config);
}

bool Cache::enabled() const {
    return impl_ && impl_->store != nullptr;
}

Task<Result<CacheEntry>> Cache::fetch(std::string key, std::string client, Producer produce) {
    if (!impl_) co_return co_await produce();

    if (impl_->store) {
        if (auto hit = co_await impl_->store->get(key)) co_return *std::move(hit);
    }

    // one producer for the key, whether or not what it makes is kept afterwards
    co_return co_await impl_->flight.run(key, [&]() -> Task<Result<CacheEntry>> {
        auto made = co_await produce();
        if (made && impl_->store && impl_->quota.mayStore(client)) {
            co_await impl_->store->put(key, *made);
        }
        co_return made;
    });
}

}
