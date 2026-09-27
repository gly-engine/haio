#pragma once

#include <haio_source.hpp>
#include <haio/internal/utils/lru.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/** reading one file out of a zip an origin holds, without unpacking the rest */
namespace Haio::Source {

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
    Detail::LruBySize<std::shared_ptr<const Archive>> held_;
};

}
