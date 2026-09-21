#include <bucket/bucket.hpp>
#include <haio_util.hpp>

#include <zlib.h>

#include <cstring>
#include <string>

namespace {

using Haio::Bytes;

constexpr uint32_t endOfDirectory = 0x06054b50;
constexpr uint32_t directoryEntry = 0x02014b50;
constexpr uint32_t localHeader    = 0x04034b50;

constexpr uint16_t methodStored  = 0;
constexpr uint16_t methodDeflate = 8;

/**
 * a zip whose entries expand far more than this is not a picture archive, it is an
 * attempt to spend the server's memory. real image data barely compresses at all, so
 * even a generous ratio is orders of magnitude away from anything legitimate.
 */
constexpr uint64_t maxExpansionRatio = 200;

uint16_t readU16LE(Bytes data, size_t off) {
    return static_cast<uint16_t>(data[off] | (data[off + 1] << 8));
}

/** the record sits at the end, behind a comment of up to 64k that has to be walked back over */
std::optional<size_t> findEndOfDirectory(Bytes data) {
    if (data.size() < 22) return std::nullopt;

    const size_t furthest = std::min<size_t>(data.size(), 22 + 0xFFFF);
    for (size_t back = 22; back <= furthest; back++) {
        const auto at = data.size() - back;
        if (Haio::Util::readU32LE(data, at) == endOfDirectory) return at;
    }
    return std::nullopt;
}

}

namespace Haio::Cdn::Bucket {

/**
 * enough of the zip format to read one file out of an archive already in memory.
 *
 * the central directory is the index the format itself keeps, so nothing has to be
 * guessed by scanning for local headers; a file that disagrees with its own directory
 * is refused rather than read around.
 *
 * @todo zip64 is not handled, so an archive over four gigabytes, or with more than
 * 65535 entries, is refused rather than misread.
 */
Result<ZipIndex> readZipIndex(Bytes data) {
    const auto end = findEndOfDirectory(data);
    if (!end) return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput, "not a zip, or its directory is missing"});

    const auto count = readU16LE(data, *end + 10);
    const auto directorySize = Util::readU32LE(data, *end + 12);
    const auto directoryAt = Util::readU32LE(data, *end + 16);

    if (directoryAt == 0xFFFFFFFF || count == 0xFFFF) {
        return std::unexpected(Haio::Error{Haio::ErrorCode::UnsupportedFormat, "zip64 archives are not supported"});
    }
    if (static_cast<uint64_t>(directoryAt) + directorySize > data.size()) {
        return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput, "the zip directory points outside the file"});
    }

    ZipIndex index;
    size_t at = directoryAt;

    for (uint16_t entry = 0; entry < count; entry++) {
        if (at + 46 > data.size() || Util::readU32LE(data, at) != directoryEntry) {
            return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput, "the zip directory is truncated"});
        }

        ZipEntry found;
        found.method = readU16LE(data, at + 10);
        found.compressed = Util::readU32LE(data, at + 20);
        found.uncompressed = Util::readU32LE(data, at + 24);
        found.at = Util::readU32LE(data, at + 42);

        const auto nameSize = readU16LE(data, at + 28);
        const auto extraSize = readU16LE(data, at + 30);
        const auto commentSize = readU16LE(data, at + 32);
        if (at + 46 + nameSize > data.size()) {
            return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput, "a zip entry name runs past the end"});
        }

        std::string name(reinterpret_cast<const char*>(data.data()) + at + 46, nameSize);
        at += 46 + nameSize + extraSize + commentSize;

        // a directory entry has nothing to read, and a name that climbs is not read at all
        if (name.empty() || name.back() == '/') continue;
        if (name.find("..") != std::string::npos) continue;

        index.entries.emplace(std::move(name), found);
    }
    return index;
}

/** inflates one entry, refusing anything that expands further than it should */
Result<std::vector<uint8_t>> readZipEntry(Bytes data, const ZipEntry& entry, size_t maxSize) {
    if (entry.uncompressed > maxSize) {
        return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput,
                                     "the file inside the zip is larger than max_unzip allows"});
    }
    if (entry.compressed > 0 && entry.uncompressed / entry.compressed > maxExpansionRatio) {
        return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput, "the file inside the zip expands too far"});
    }

    if (entry.at + 30 > data.size() || Util::readU32LE(data, entry.at) != localHeader) {
        return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput, "a zip entry has no header where the directory says"});
    }

    // the local header repeats the name and may carry different extra data, so its
    // own lengths are the ones that count rather than the directory's
    const auto nameSize = readU16LE(data, entry.at + 26);
    const auto extraSize = readU16LE(data, entry.at + 28);
    const auto from = static_cast<size_t>(entry.at) + 30 + nameSize + extraSize;
    if (from + entry.compressed > data.size()) {
        return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput, "a zip entry runs past the end of the file"});
    }

    if (entry.method == methodStored) {
        if (entry.compressed != entry.uncompressed) {
            return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput, "a stored zip entry disagrees with its own size"});
        }
        return std::vector<uint8_t>(data.begin() + from, data.begin() + from + entry.compressed);
    }
    if (entry.method != methodDeflate) {
        return std::unexpected(Haio::Error{Haio::ErrorCode::UnsupportedFormat, "the zip entry uses a compression haio cannot read"});
    }

    std::vector<uint8_t> out(entry.uncompressed);

    z_stream stream{};
    // a negative window means raw deflate, which is what a zip stores: no zlib header
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        return std::unexpected(Haio::Error{Haio::ErrorCode::Internal, "cannot start inflate"});
    }

    stream.next_in = const_cast<Bytef*>(data.data() + from);
    stream.avail_in = static_cast<uInt>(entry.compressed);
    stream.next_out = out.data();
    stream.avail_out = static_cast<uInt>(out.size());

    const auto status = inflate(&stream, Z_FINISH);
    const auto written = stream.total_out;
    inflateEnd(&stream);

    if (status != Z_STREAM_END || written != entry.uncompressed) {
        return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput, "the file inside the zip could not be inflated"});
    }
    return out;
}

}
