#pragma once

#include "haio.hpp"
#include "haio/parser.hpp"
#include "haio_cache.hpp"
#include "haio_platform.hpp"
#include "haio_source.hpp"

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

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
 * @defgroup cdn_query Query
 * @ingroup cdn
 * what to serve, written as a line rather than as a path.
 *
 * the path says only that this is the cdn. the query is the conversion, one word per
 * "&", and the order they are written in is the order they happen:
 *
 * @code
 * /cdn?assets:logo.png
 * /cdn?assets:logo.png&resize=64x64&format=png
 * /cdn?text:ola+mundo&format=png
 * @endcode
 *
 * these are the same words Haio::Parser reads for the command line, so a request and
 * a `haio convert` line are the same conversion by construction rather than by two
 * parsers agreeing with each other. anything the one can say, the other can.
 *
 * ## Query Words
 *
 * | written           | is                                             |
 * | ----------------- | ---------------------------------------------- |
 * | `assets:logo.png` | a source: a bucket's name, then the path in it |
 * | `text:oi`         | a source a brush paints, which reads nothing   |
 * | `resize=64x64`    | a stage, or an option for the stage after it   |
 * | `negate`          | a stage that takes no value                    |
 * | `begin` … `end`   | a parenthesis, and `(` … `)` means the same    |
 *
 * a source always carries a bucket: there is no bucket that is assumed, so nothing is
 * served by accident. where a word has both a ":" and an "=", whichever comes first
 * decides what it is -- "text:a=b" is a brush painting "a=b", and
 * "define=png:compression-level=9" is an option whose value has a colon in it.
 *
 * a "+" is a space, the way a form writes one, so "?text:ola+mundo" paints two words
 * and "%20" says the same thing. the one value that spelled a plus on purpose is a
 * geometry, and those read a space as the plus it came from: "?crop=4x4+2+1" is the
 * rectangle a command line calls "4x4+2+1". a plus that has to stay one is "%2B".
 *
 * ## Query Order
 *
 * an option belongs to the stage written after it, exactly as "-filter bayer -palette
 * cga" does on a command line. so "?size=8x8&xc:red" paints an eight by eight red,
 * and "?xc:red&size=8x8" has nothing left to take the size and is refused.
 *
 * the order of the stages is the order of the work, which is why the query is read as
 * words and not as a map: "crop then resize" and "resize then crop" are two different
 * pictures, and a map would have thrown that away.
 *
 * ## Query Answer
 *
 * `format=` says what container to write; without it the answer keeps the one the
 * first source arrived in, and a brush that arrived in none is written as png.
 *
 * `name=` says what the answer is called, in `content-disposition` and in whatever an
 * encoding writes into the bytes; without it the first source's own name is used.
 * `encoding=` says how the bytes are written, for a client that cannot take every one
 * of them; see @ref Haio::BodyEncoding.
 *
 * asking for the format a file already is, with nothing else to do to it, hands back
 * the stored bytes rather than a re-encode that merely has the same pixels.
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
 * ## Bucket Archive
 *
 * a path may name a file inside an archive when `allow_unzip` is on, and a file
 * inside an archive inside that one:
 *
 * @code
 * /cdn?assets:pack.zip/logo.png
 * /cdn?assets:foo.ipk/data.tar.gz/assets/icon80x80.png
 * @endcode
 *
 * the first segment named like an archive (`.zip`, `.ipk`, `.deb`, `.tar`, `.tgz`,
 * `.gz`) is the file the bucket holds. what it is, though, is read from its bytes:
 * zip, ar (which is what an ipk and a deb are), tar, or any of them under gzip. inside
 * it the path is walked a segment at a time, and every entry on the way that is an
 * archive itself is opened in turn.
 *
 * every archive opened is kept in memory, each level on its own, so a second icon out
 * of the same `data.tar.gz` costs neither the fetch nor the gunzip.
 *
 * a file larger than `max_unzip`, or one that expands more than two hundred times, is
 * refused, and a gzip is held to the same: real image data barely compresses, so
 * anything near that is an attempt to spend memory rather than an archive.
 *
 * ## Bucket Http
 *
 * @code{.toml}
 * [bucket.upstream]
 * url = "https://host/prefix"   # everything under one prefix
 * url = "s3://s3.eu-west-1.amazonaws.com/bucket"   # signed, with region beside it
 * url = "https://*"             # open: the request names the host
 * @endcode
 *
 * redirects are followed, downgrades included. an open bucket fetches whatever host
 * the caller names, warns at startup, and is meant for testing.
 *
 * ## Bucket S3
 *
 * an `s3://` bucket is https with a signature, and the signature is made over a
 * region: the host only says where the connection goes, so the region is written
 * next to the url. a bucket that signs and has none is refused at startup rather
 * than at its first request, which would come back as a 403 that says nothing about
 * why. one that does not sign needs no region at all.
 *
 * amazon names the region in the host too, `bucket.s3.eu-west-1.amazonaws.com`, and
 * the two have to agree. minio takes `us-east-1` unless it was set up otherwise, and
 * r2 takes `auto`.
 *
 * @code{.toml}
 * [bucket.orders]
 * url = "s3://bucket.s3.us-east-1.amazonaws.com/prefix"
 * region = "us-east-1"    # needed once it signs
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
 * strict = false                 # a limit left out stops the server instead of warning
 * timeout = 10                   # in seconds, one conversion, zero is forever
 * max_size_pixel = 4096          # in pixels, what a resize may ask for outright
 * max_size_percent = 500         # in percent, what a resize may ask for as a share
 * max_cache_entries_by_ip = 30   # entries one address may create per ttl window
 * max_requests_by_ip = 10        # per second, as a leaky bucket
 * allow_unzip = false            # whether a path may reach inside an archive
 * max_unzip = 64                 # in mb, one extracted file and the archives kept
 * max_sources = 4                # how many files one request may ask for, zero is every
 * max_operations = 64            # a source costs two, a stage one, zero is every
 * @endcode
 *
 * ## Security Defaults
 *
 * every one of them is off until the config says a number: a limit picked blind shuts
 * out an office behind one NAT or refuses a page nobody meant to refuse, and what a
 * deployment can afford is the deployment's to say. the server names at startup each
 * one left out, and `strict = true` turns those lines into a refusal to start.
 *
 * max_unzip is the one exception, at 64mb. undoing an archive is the only work here
 * that a small request can turn into an arbitrarily large one, so it is never
 * unbounded: zero is refused rather than taken as no limit.
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
 * | `max_unzip`               | a small archive that expands into a large one    |
 * | `max_sources`             | one short url turning into many upstream fetches |
 * | `max_operations`          | one short url turning into a long pipeline       |
 *
 * over the cache quota the answer is still produced and still correct, it is simply
 * not stored: the limit is on eviction, not on access.
 */

namespace Haio::Cdn {

/**
 * a bucket is an origin with a name, read from [bucket.<name>]; see
 * Haio::Source::Origin for what its url may say.
 */
using BucketConfig = Source::Origin;

/**
 * every limit the server has, in one table, so that reviewing what protects this
 * deployment is reading one page of the config rather than hunting for it.
 *
 * each one is safe when left out, and the server says at startup which ones it chose
 * for itself; see @ref cdn_security.
 */
struct SecurityConfig {
    bool strict = false;
    std::chrono::seconds timeout{0};
    int maxSizePixel = 0;
    int maxSizePercent = 0;
    size_t maxCacheEntriesByIp = 0;
    size_t maxRequestsByIp = 0;
    bool allowUnzip = false;
    size_t maxUnzip = 64u << 20;
    size_t maxSources = 0;
    size_t maxOperations = 0;
};

inline constexpr std::string_view warningTag = "\x1b[41;97m[warning]\x1b[0m ";

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

/**
 * the response says which Haio::BodyEncoding the body is in with `x-encoding`. it is
 * not content-encoding on purpose: that header is for compression a client undoes on
 * its own, and one that met "base64" there would fail the response rather than hand
 * the text over.
 *
 * `x-original-content-length` is always sent: the body's size before gzip.
 */

/** the query as the words Haio::Parser reads, one per "&", in the order written */
std::vector<Parser::Word> wordsOf(std::string_view query);

Config parseConfig(std::string_view text);
Config loadConfig(const std::filesystem::path& path);
Task<Result<Blob>> fetchBucket(const Config& config, Source::Archives& archives, std::string bucket, std::string path);

/** the cdn as one function, with the cache, the limiter and the archives made once behind it */
Platform::HttpHandler makeHandler(Config config);

/** listens until the platform's loop is stopped, and throws if it cannot listen at all */
Task<void> runServer(Config config);

}
