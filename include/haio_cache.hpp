#pragma once

#include "haio.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace Haio::Cdn {

/**
 * what the cache holds is a finished response, not an image: the work being saved is
 * the fetch and the whole conversion behind it, so replaying an entry costs a copy.
 */
struct CacheEntry {
    std::vector<uint8_t> data;
    std::string contentType;
    std::string filename;
};

struct CacheConfig {
    std::chrono::seconds ttl{0};   // zero leaves the cache off
    size_t maxUsage = 64u << 20;   // bytes the store may hold, whichever store it is
    std::string url;               // empty keeps entries in this process
};

/**
 * a backend is chosen at runtime from the scheme of the url, which is why this is a
 * virtual interface rather than a concept the way the codecs do it: the answer is in
 * a config file, not in the build.
 */
class CacheStore {
public:
    virtual ~CacheStore() = default;
    /**
     * a hit renews the entry's ttl, so what is being asked for keeps living and what
     * nobody wants ages out. the window is therefore since the last read, not since
     * the write: a popular creative is never rebuilt on a clock.
     */
    virtual boost::asio::awaitable<std::optional<CacheEntry>> get(const std::string& key) = 0;
    virtual boost::asio::awaitable<void> put(const std::string& key, const CacheEntry& entry) = 0;
};

/**
 * the md5 of the bucket, the path and the query, in the order they were written.
 *
 * the query is not sorted, and must not be: it spells out a pipeline, so crop before
 * resize is a different picture from resize before crop. sorting would collide two
 * different requests onto one entry and serve the wrong image, which is a worse fault
 * than keeping two entries for one answer. the method is left out so a HEAD finds
 * what a GET stored.
 */
std::string cacheKey(std::string_view bucket, std::string_view path, std::string_view query);

/** the framing the file and redis backends share; the memory one keeps the struct */
std::vector<uint8_t> encodeEntry(const CacheEntry& entry);
std::optional<CacheEntry> decodeEntry(std::span<const uint8_t> raw);

std::unique_ptr<CacheStore> makeMemoryStore(size_t maxUsage, std::chrono::seconds ttl);
std::unique_ptr<CacheStore> makeFileStore(std::filesystem::path root, std::chrono::seconds ttl, size_t maxUsage);
std::unique_ptr<CacheStore> makeRedisStore(std::string url, std::chrono::seconds ttl, size_t maxUsage, boost::asio::any_io_executor executor);

/**
 * the store, plus the part that keeps one key from being produced many times at once.
 *
 * the two are independent on purpose: deduplication needs no store and stays on even
 * when no cache is configured, because a burst on one key sending one upstream
 * request per caller is an amplifier whether or not the answer is kept afterwards.
 */
class Cache {
public:
    using Producer = std::function<boost::asio::awaitable<Result<CacheEntry>>()>;

    Cache() = default;
    /**
     * maxEntriesByIp is spelled in [security] rather than in [cache], because it is a
     * limit on a caller and not on the store. it is passed rather than copied into
     * CacheConfig so that the number lives in exactly one place.
     */
    Cache(CacheConfig config, size_t maxEntriesByIp, boost::asio::any_io_executor executor);

    /** true when entries are kept; deduplication happens either way */
    bool enabled() const;

    /**
     * returns the stored entry, or runs the producer once and stores what it made.
     *
     * client is the address the request came from, and it is only used to decide
     * whether the result may be stored. reading is always free: the limit is there to
     * stop one caller from filling the cache with keys nobody else will ask for, and
     * evicting everyone else's entries on the way.
     */
    boost::asio::awaitable<Result<CacheEntry>> fetch(std::string key, std::string client, Producer produce);

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

}
