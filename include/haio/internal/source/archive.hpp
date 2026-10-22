#pragma once

#include <haio_source.hpp>
#include <haio/internal/utils/lru.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

/**
 * reading one file out of an archive, and out of an archive inside that one, without
 * unpacking anything else: "foo.ipk/data.tar.gz/assets/icon.png" is an ar holding a
 * gzip holding a tar holding the picture.
 *
 * each format only builds an index -- where every file sits -- and the one file asked
 * for is read out of it. a tar or an ar stores its files as they are, so an entry is
 * a range; a zip compresses them one by one, so an entry is found through the zip's
 * own local header. a gzip is not an archive at all, only a wrapper, and is undone
 * whole before the archive inside it is indexed.
 */
namespace Haio::Source {

/** where one file sits inside an archive */
struct ArchiveEntry {
    uint64_t at = 0;
    uint64_t compressed = 0;
    uint64_t uncompressed = 0;
    /** zip only: 0 stored, 8 deflate */
    uint16_t method = 0;
    /** at is a zip local header rather than the first byte of the file */
    bool zip = false;
};

struct ArchiveIndex {
    std::map<std::string, ArchiveEntry> entries;
};

/**
 * an archive whose files expand far more than this is not a picture archive, it is an
 * attempt to spend the server's memory. real image data barely compresses at all, so
 * even a generous ratio is orders of magnitude away from anything legitimate.
 */
constexpr uint64_t maxExpansionRatio = 200;

Result<ArchiveIndex> readZipIndex(Bytes data);
Result<std::vector<uint8_t>> readZipEntry(Bytes data, const ArchiveEntry& entry, size_t maxSize);
Result<ArchiveIndex> readTarIndex(Bytes data);
Result<ArchiveIndex> readArIndex(Bytes data);

/** a whole gzip stream undone, refusing one that grows past maxSize or expands too far */
Result<std::vector<uint8_t>> gunzip(Bytes data, size_t maxSize);

/** an archive in memory with its index; a .gz is kept already undone */
struct Archive {
    std::vector<uint8_t> data;
    ArchiveIndex index;
};

/**
 * the bytes read as whichever archive they are. the bytes decide, not the name: an
 * ipk is an ar on webos and a tar.gz on older opkg, and both are called .ipk.
 */
Result<Archive> openArchive(std::vector<uint8_t> data, std::string_view name, size_t maxSize);

/** one file out of an archive, as long as it fits in maxSize */
Result<std::vector<uint8_t>> readEntry(const Archive& archive, const ArchiveEntry& entry, size_t maxSize);

/**
 * the archives themselves, kept apart from the response cache because what they hold
 * is not an answer: a second picture out of the same archive should not have to fetch
 * or inflate it again. every level is kept on its own, so a second icon out of
 * foo.ipk/data.tar.gz costs neither the ipk nor the gunzip.
 */
class Archives {
public:
    explicit Archives(size_t maxUsage);

    std::shared_ptr<const Archive> find(const std::string& key);
    std::shared_ptr<const Archive> keep(const std::string& key, Archive archive);

private:
    Detail::LruBySize<std::shared_ptr<const Archive>> held_;
};

/**
 * the file inside names, read out of archive and out of every archive on the way
 * there. key is what archive is kept as, and the ones inside it are kept under key
 * plus their own name.
 */
Result<Blob> digArchive(std::shared_ptr<const Archive> archive, const std::string& key, std::string_view inside,
                        const ArchiveOptions& options);

}
