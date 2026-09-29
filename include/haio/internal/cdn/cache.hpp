#pragma once

#include <haio_cache.hpp>
#include <haio/internal/cdn/clients.hpp>

#include <chrono>
#include <map>
#include <memory>
#include <string>

/** the pieces the cache is made of, none of which anything outside it needs */
namespace Haio::Cdn::Detail {

/**
 * one producer per key, however many callers ask at once.
 *
 * it knows nothing about caching: a burst on a key reaches the producer once whether
 * or not the answer is kept afterwards, which is why turning the cache off does not
 * turn an amplifier back on.
 */
class SingleFlight {
public:
    using Producer = std::function<Task<Result<CacheEntry>>()>;

    /** runs the producer, or waits for whoever is already running it for this key */
    Task<Result<CacheEntry>> run(const std::string& key, const Producer& produce);

private:
    struct Pending;
    std::map<std::string, std::shared_ptr<Pending>> inFlight_;
};

/**
 * how many entries one address may create per window.
 *
 * the limit is on creating, never on reading: what it answers is one caller filling
 * the cache with keys nobody else wants and evicting everyone else on the way.
 */
class ClientQuota {
public:
    ClientQuota(size_t perWindow, std::chrono::seconds window);

    bool mayStore(const std::string& client);

private:
    struct Window {
        size_t created = 0;
        std::chrono::steady_clock::time_point resets;
    };

    size_t perWindow_;
    std::chrono::seconds window_;
    ClientTable<Window> seen_;
};

}
