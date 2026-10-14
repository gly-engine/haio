#include <haio/implementation/cdn.hpp>
#include <haio/implementation/platform.hpp>

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Haio {

/**
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
 *
 * # Bucket
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
 * /cdn/assets/pack.zip/logo.png
 * /cdn/assets/foo.ipk/data.tar.gz/assets/icon80x80.png
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
 *
 * # Cache
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
 *
 * # Security
 * every limit in one table, so reviewing what protects a deployment is one page.
 *
 * @code{.toml}
 * [security]
 * timeout = 10                   # in seconds, one conversion
 * max_size_pixel = 4096          # in pixels, what a resize may ask for outright
 * max_size_percent = 500         # in percent, what a resize may ask for as a share
 * max_cache_entries_by_ip = 30   # entries one address may create per ttl window
 * max_requests_by_ip = 10        # per second, as a leaky bucket
 * allow_unzip = false            # whether a path may reach inside an archive
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
 * | `max_unzip`               | a small archive that expands into a large one    |
 *
 * over the cache quota the answer is still produced and still correct, it is simply
 * not stored: the limit is on eviction, not on access.
 */
int commandCdn(int argc, char* argv[]) {
    std::filesystem::path configPath;

    for (int i = 1; i < argc; i++) {
        const std::string_view arg = argv[i];
        if (arg.starts_with('-')) throw std::runtime_error("unknown cdn option: " + std::string(arg));
        if (!configPath.empty()) throw std::runtime_error("cdn takes a single config file");
        configPath = arg;
    }

    Implementation::Cdn::Config config;
    if (!configPath.empty()) {
        config = Implementation::Cdn::loadConfig(configPath);
    } else if (const char* inlineToml = std::getenv("HAIO_CDN_TOML")) {
        config = Implementation::Cdn::parseConfig(inlineToml);
    } else {
        throw std::runtime_error("cdn needs a config file, or HAIO_CDN_TOML holding the config itself");
    }

    Implementation::Interfaces::stopOnSignal();
    Implementation::Interfaces::run(Implementation::Cdn::runServer(std::move(config)));
    return 0;
}

}
