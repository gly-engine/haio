#pragma once

#include <haio_cdn.hpp>

#include <boost/asio/awaitable.hpp>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * one directory per way of reaching bytes, the way the codecs do it. the dispatcher in
 * buckets.cpp picks by the scheme the config parsed, and each fetcher only knows its
 * own protocol.
 */
namespace Haio::Cdn::Bucket {

/**
 * carries why it failed, so the http layer does not have to guess from the text.
 *
 * it is not called Error because Haio::Error already is, and the two are different
 * things: this one is thrown between the fetchers, that one is returned to a caller.
 */
struct Failure : std::runtime_error {
    Haio::ErrorCode code;
    Failure(Haio::ErrorCode why, const std::string& what) : std::runtime_error(what), code(why) {}
};

/** file:// buckets: a directory on this machine, and never a step above it */
Blob fetchFile(const BucketConfig& bucket, std::string path);

/** http, https and s3 buckets, open or with a fixed endpoint */
boost::asio::awaitable<Blob> fetchHttp(const BucketConfig& bucket, std::string path);

/** where one file sits inside an archive, as the zip's own directory records it */
struct ZipEntry {
    uint32_t at = 0;
    uint32_t compressed = 0;
    uint32_t uncompressed = 0;
    uint16_t method = 0;
};

struct ZipIndex {
    std::map<std::string, ZipEntry> entries;
};

Result<ZipIndex> readZipIndex(Bytes data);
Result<std::vector<uint8_t>> readZipEntry(Bytes data, const ZipEntry& entry, size_t maxSize);

/**
 * "pack.zip/logo.png" split into the archive and the name inside it, or nothing when
 * the path names no archive. the split is on the first ".zip/", so an archive inside
 * an archive is read as a file inside the outer one rather than opened again.
 */
struct ZipPath {
    std::string archive;
    std::string inside;
};
std::optional<ZipPath> splitZipPath(std::string_view path);

class ZipArchives;
boost::asio::awaitable<Blob> fetchInsideZip(const BucketConfig& bucket, const ZipPath& path,
                                            const SecurityConfig& security, ZipArchives& archives);

/**
 * the archives themselves, kept apart from the response cache because what they hold
 * is not an answer: a request for a second picture out of the same zip should not
 * have to fetch it again, and a request for the same picture never reaches here at
 * all, since the response cache already answered.
 */
class ZipArchives {
public:
    explicit ZipArchives(size_t maxUsage);

    struct Archive {
        std::vector<uint8_t> data;
        ZipIndex index;
    };

    std::shared_ptr<const Archive> find(const std::string& key);
    std::shared_ptr<const Archive> keep(const std::string& key, std::vector<uint8_t> data, ZipIndex index);

private:
    struct Held {
        std::string key;
        std::shared_ptr<const Archive> archive;
        size_t size = 0;
    };

    size_t maxUsage_;
    size_t held_ = 0;
    std::list<Held> order_;
    std::map<std::string, std::list<Held>::iterator> index_;
};

}
