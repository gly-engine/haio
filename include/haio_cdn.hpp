#pragma once

#include "haio.hpp"
#include "haio_cache.hpp"

#include <boost/asio/awaitable.hpp>

#include <chrono>
#include <filesystem>
#include <map>
#include <set>
#include <string_view>

/**
 * @defgroup cdn CDN
 * serving images over http.
 *
 * one toml file, named on the command line or handed over whole in HAIO_CDN_TOML.
 * every setting lives in a table; a key outside one is refused.
 *
 * @code{.toml}
 * [cdn]
 * host = "0.0.0.0"   # address to listen on
 * port = 8080
 * @endcode
 */

/**
 * @defgroup cdn_bucket Bucket
 * @ingroup cdn
 * where the bytes come from. the scheme of the url picks the reader, so there is no
 * type to set. `url` is the only key a bucket takes.
 *
 * ## Bucket File
 *
 * @code{.toml}
 * [bucket.assets]
 * url = "file://assets"     # relative to the working directory
 * url = "file:///srv/img"   # absolute
 * @endcode
 *
 * ".." and a segment starting with "~" are refused.
 *
 * ## Bucket Zip
 *
 * a path may name a file inside an archive, as in `/cdn/assets/pack.zip/logo.png`,
 * when `allow_unzip` is on. the archive is read once and kept in memory, so asking
 * for a second picture out of it costs no fetch.
 *
 * an entry larger than `max_unzip`, or one that expands more than two hundred times,
 * is refused: real image data barely compresses, so anything near that is an attempt
 * to spend memory rather than an archive.
 *
 * ## Bucket Http
 *
 * @code{.toml}
 * [bucket.upstream]
 * url = "https://host/prefix"   # everything under one prefix
 * url = "s3://s3.eu-west-1.amazonaws.com/bucket"   # signed, region from the host
 * url = "s3://minio.local:9000/bucket?region=us-east-1"   # or said outright
 * url = "https://*"             # open: the request names the host
 * @endcode
 *
 * redirects are followed, downgrades included. an open bucket fetches whatever host
 * the caller names, warns at startup, and is meant for testing.
 *
 * ## Bucket S3
 *
 * an `s3://` bucket is https with a signature. the region is settled while the config
 * loads, from `?region=` if it says so, else from an amazon host, else from
 * `AWS_DEFAULT_REGION`. a bucket whose region nobody can work out is refused at
 * startup rather than at the first request.
 *
 * @code{.toml}
 * [bucket.orders]
 * url = "s3://bucket.s3.us-east-1.amazonaws.com/prefix"
 * access_key = "AKIA..."
 * secret_key = "..."
 * session_token = "..."   # only for temporary credentials
 * @endcode
 *
 * the keys may be left out, and then AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY and
 * AWS_SESSION_TOKEN are read instead. with neither the request goes out unsigned,
 * which is what a public bucket wants.
 *
 * a config holding a secret is a file worth guarding, and haio says so at startup if
 * anybody but its owner can read it.
 *
 * an s3 bucket cannot be open: a signature is made for one host, and there is nothing
 * to sign for a host that arrives with the request.
 */

/**
 * @defgroup cdn_cache Cache
 * @ingroup cdn
 * keeping the answer, so the fetch and the conversion behind it happen once.
 *
 * @code{.toml}
 * [cache]
 * ttl = 5000         # in seconds, since the entry was last wanted
 * max_usage = 50     # in mb, wherever the entries are kept
 * url = "redis://127.0.0.1:6379"   # optional; without it, kept in this process
 * @endcode
 *
 * ## Cache Key
 *
 * the md5 of the bucket, the path and the query, in the order written. the query is
 * not sorted: it will spell a pipeline once filters chain, and a sorted key would
 * serve one picture when another was asked for. the method is left out, so a HEAD
 * finds what a GET stored.
 *
 * ## Cache Where
 *
 * | url              | kept in          | bounded by          |
 * | ---------------- | ---------------- | ------------------- |
 * | absent           | this process     | what it holds       |
 * | `file://dir`     | one file per key | what the files take |
 * | `redis://host`   | a shared redis   | an index haio keeps |
 *
 * a redis that cannot be reached reads as a miss: losing the cache costs latency and
 * nothing else.
 *
 * ## Cache Flow
 *
 * deduplication is not part of caching. with no `[cache]` nothing is kept, and a
 * burst on one key still reaches the producer once.
 *
 * @startuml
 * start
 * if (cache on?) then (yes)
 *   :look in the store;
 *   if (hit?) then (yes)
 *     :renew the ttl;
 *     stop
 *   endif
 * endif
 * if (already being produced?) then (yes)
 *   :wait for the leader;
 *   :take their answer;
 *   stop
 * else (no)
 *   :become the leader;
 * endif
 * :fetch and convert;
 * if (cache on and within quota?) then (yes)
 *   :store;
 * endif
 * :wake the waiters;
 * stop
 * @enduml
 */

/**
 * @defgroup cdn_security Security
 * @ingroup cdn
 * every limit in one table, so reviewing what protects a deployment is one page.
 *
 * @code{.toml}
 * [security]
 * timeout = 10                   # in seconds, one conversion
 * max_size_pixel = 4096          # in pixels, what a resize may ask for outright
 * max_size_percent = 500         # in percent, what a resize may ask for as a share
 * max_cache_entries_by_ip = 30   # entries one address may create per ttl window
 * max_requests_by_ip = 10        # per second, as a leaky bucket
 * allow_unzip = false            # whether a path may reach inside a zip
 * max_unzip = 64                 # in mb, one extracted file and the archives kept
 * @endcode
 *
 * ## Security Defaults
 *
 * each has a default and the server starts without them. the two counted per address
 * default to no limit on purpose: a number picked blind shuts out an office behind
 * one NAT. either way the server says at startup which ones it chose for itself.
 *
 * ## Security Why
 *
 * | key                       | answers                                          |
 * | ------------------------- | ------------------------------------------------ |
 * | `max_size_pixel`          | a url asking for an allocation                   |
 * | `max_size_percent`        | the same, asked for as a share of the picture    |
 * | `timeout`                 | a url asking for work                            |
 * | `max_cache_entries_by_ip` | one caller evicting everyone else's entries      |
 * | `max_requests_by_ip`      | a flood                                          |
 * | `allow_unzip`             | work on behalf of whoever wrote the file         |
 * | `max_unzip`               | a small zip that expands into a large one        |
 *
 * over the cache quota the answer is still produced and still correct, it is simply
 * not stored: the limit is on eviction, not on access.
 */

namespace Haio::Cdn {

/**
 * a bucket is described entirely by the scheme of its url:
 *
 *   file://relative/dir     file:///absolute/dir
 *   http://host/prefix      https://host/prefix      s3://host/bucket
 *   https://\*               open, the request names the host
 *   //\*                     open, the request names the scheme and the host
 */
struct BucketConfig {
    std::string name;
    std::string url;

    // derived from the url while the config loads, so a bad one fails at startup
    std::string scheme;
    bool open = false;
    std::filesystem::path root;

    /** s3 only: from ?region=, then the host, then AWS_DEFAULT_REGION */
    std::string region;

    /**
     * s3 only, and the reason a config file deserves careful permissions. empty
     * leaves the request unsigned, which is what a public bucket wants.
     */
    std::string accessKey;
    std::string secretKey;
    std::string sessionToken;
};

/**
 * every limit the server has, in one table, so that reviewing what protects this
 * deployment is reading one page of the config rather than hunting for it.
 *
 * each one is safe when left out, and the server says at startup which ones it chose
 * for itself; see @ref cdn_security.
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

    /** whether a path may reach inside a zip; off unless asked for */
    bool allowUnzip = false;

    /**
     * bytes one file out of a zip may take, and what the archives kept in memory may
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
namespace Bucket { class ZipArchives; }

boost::asio::awaitable<Result<Blob>> fetchBucket(const Config& config, Bucket::ZipArchives& archives, std::string bucket, std::string path);
boost::asio::awaitable<void> runServer(Config config);

}
