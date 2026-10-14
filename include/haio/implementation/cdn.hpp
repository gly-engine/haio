#pragma once

#include "haio/implementation/error.hpp"
#include "haio/implementation/image.hpp"
#include "haio/implementation/platform/task.hpp"
#include "haio/implementation/cdn/cache.hpp"
#include "haio/implementation/source.hpp"

#include <chrono>
#include <filesystem>
#include <map>
#include <set>
#include <string_view>

namespace Haio::Implementation::Cdn {

/**
 * a bucket is an origin with a name, read from [bucket.<name>]; see
 * Haio::Implementation::Source::Origin for what its url may say.
 */
using BucketConfig = Source::Origin;

/**
 * every limit the server has, in one table, so that reviewing what protects this
 * deployment is reading one page of the config rather than hunting for it.
 *
 * each one is safe when left out, and the server says at startup which ones it chose
 * for itself; see Haio::commandCdn.
 */
struct SecurityConfig {
    /** seconds one conversion may run before it is given up on */
    std::chrono::seconds timeout{10};

    /** the largest width or height a resize may ask for outright */
    int maxSizePixel = 4096;

    /**
     * the largest share a resize may ask for, as a percentage.
     *
     * it is a separate number because it bounds a different thing: a picture scaled
     * by a share is bounded by what it was, so five hundred percent of a thumbnail is
     * still small, while five hundred percent of something large is not.
     */
    int maxSizePercent = 500;

    /** entries one address may create per cache window; zero is no limit */
    size_t maxCacheEntriesByIp = 0;

    /** requests one address may make per second; zero is no limit */
    size_t maxRequestsByIp = 0;

    /** whether a path may reach inside an archive; off unless asked for */
    bool allowUnzip = false;

    /**
     * bytes one file out of an archive may take, and what the archives kept in memory may
     * take altogether. megabytes in the config.
     */
    size_t maxUnzip = 64u << 20;
};

struct Config {
    std::string host = "0.0.0.0";
    unsigned short port = 8080;

    SecurityConfig security;
    CacheConfig cache;
    std::map<std::string, BucketConfig> buckets;

    /**
     * the keys the file actually spelled out, as "max_size" or "cache.max_ip".
     *
     * the safety limits all have a default, so leaving one out is safe rather than
     * open; this is what lets the server say which ones it chose for itself instead
     * of letting an operator believe they had configured something.
     */
    std::set<std::string> declared;
};

Config parseConfig(std::string_view text);
Config loadConfig(const std::filesystem::path& path);
Platform::Task<Result<Blob>> fetchBucket(const Config& config, Source::Archives& archives, std::string bucket, std::string path);

/** listens until the platform's loop is stopped, and throws if it cannot listen at all */
Platform::Task<void> runServer(Config config);

}
