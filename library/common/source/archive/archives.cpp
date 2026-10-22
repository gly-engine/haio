#include <haio/internal/source/archive.hpp>

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>

namespace {

using Haio::Bytes;

/** what a segment is called when it is a file to open rather than a directory to walk */
constexpr std::array archiveExtensions{".zip", ".ipk", ".deb", ".tar", ".tgz", ".gz"};

bool namedLikeArchive(std::string_view segment) {
    std::string lower(segment);
    std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return std::ranges::any_of(archiveExtensions, [&](std::string_view extension) { return lower.ends_with(extension); });
}

bool startsWith(Bytes data, std::string_view prefix) {
    return data.size() >= prefix.size()
        && std::equal(prefix.begin(), prefix.end(), data.begin(), [](char a, uint8_t b) { return static_cast<uint8_t>(a) == b; });
}

bool isGzip(Bytes data) {
    return data.size() >= 2 && data[0] == 0x1f && data[1] == 0x8b;
}

}

namespace Haio::Source {

std::optional<ArchivePath> splitArchivePath(std::string_view path) {
    size_t from = 0;
    while (true) {
        const auto slash = path.find('/', from);
        if (slash == std::string_view::npos) return std::nullopt;

        if (namedLikeArchive(path.substr(from, slash - from))) {
            const auto inside = path.substr(slash + 1);
            if (inside.empty()) return std::nullopt;
            return ArchivePath{std::string(path.substr(0, slash)), std::string(inside)};
        }
        from = slash + 1;
    }
}

/**
 * undone into a buffer that grows as it needs to, since a gzip only states its size
 * modulo four gigabytes, at the end, and it may lie. the limit and the ratio are the
 * same ones a zip entry is held to.
 */
Result<std::vector<uint8_t>> gunzip(Bytes data, size_t maxSize) {
    z_stream stream{};
    // 16 over the window asks zlib for the gzip wrapper rather than its own
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) HAIO_FAIL(Internal, "cannot start inflate");

    stream.next_in = const_cast<Bytef*>(data.data());
    stream.avail_in = static_cast<uInt>(data.size());

    std::vector<uint8_t> out;
    int status = Z_OK;
    while (status == Z_OK) {
        if (out.size() >= maxSize) break;
        const auto had = out.size();
        out.resize(std::min(maxSize, std::max<size_t>(had * 2, 64u << 10)));

        stream.next_out = out.data() + had;
        stream.avail_out = static_cast<uInt>(out.size() - had);
        status = inflate(&stream, Z_NO_FLUSH);
        out.resize(out.size() - stream.avail_out);
    }
    inflateEnd(&stream);

    if (status != Z_STREAM_END) {
        if (status == Z_OK) HAIO_FAIL(InvalidInput, "the gzip undoes to more than one extracted file may be");
        HAIO_FAIL(InvalidInput, "the gzip is damaged or cut short");
    }
    if (!data.empty() && out.size() / data.size() > maxExpansionRatio) {
        HAIO_FAIL(InvalidInput, "the gzip expands too far");
    }
    return out;
}

namespace {

Result<Archive> openAs(std::vector<uint8_t> data, std::string_view name, size_t maxSize, bool mayGunzip) {
    Result<ArchiveIndex> index = std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                                       std::string(name) + " is not an archive haio can open"});

    if (startsWith(data, "PK\x03\x04") || startsWith(data, "PK\x05\x06")) {
        index = readZipIndex(data);
    } else if (startsWith(data, "!<arch>\n")) {
        index = readArIndex(data);
    } else if (isGzip(data) && mayGunzip) {
        auto undone = gunzip(data, maxSize);
        if (!undone) return std::unexpected(undone.error());
        // what a gzip holds is read the same way, only it may not be another gzip
        return openAs(*std::move(undone), name, maxSize, false);
    } else if (data.size() >= 512) {
        // a tar has no magic worth trusting, since the oldest carry none; its header
        // checksum is what tells it apart, and readTarIndex checks exactly that
        index = readTarIndex(data);
        if (!index) index = std::unexpected(Error{ErrorCode::UnsupportedFormat, std::string(name) + " is not an archive haio can open"});
    }

    if (!index) return std::unexpected(index.error());
    return Archive{std::move(data), *std::move(index)};
}

}

Result<Archive> openArchive(std::vector<uint8_t> data, std::string_view name, size_t maxSize) {
    return openAs(std::move(data), name, maxSize, true);
}

Result<std::vector<uint8_t>> readEntry(const Archive& archive, const ArchiveEntry& entry, size_t maxSize) {
    if (entry.zip) return readZipEntry(archive.data, entry, maxSize);

    if (entry.uncompressed > maxSize) HAIO_FAIL(InvalidInput, "the file inside the archive is larger than one extracted file may be");
    if (entry.at > archive.data.size() || entry.uncompressed > archive.data.size() - entry.at) {
        HAIO_FAIL(InvalidInput, "an archive entry runs past the end of the file");
    }
    const auto from = archive.data.begin() + static_cast<std::ptrdiff_t>(entry.at);
    return std::vector<uint8_t>(from, from + static_cast<std::ptrdiff_t>(entry.uncompressed));
}

Archives::Archives(size_t maxUsage) : held_(maxUsage) {}

std::shared_ptr<const Archive> Archives::find(const std::string& key) {
    auto* found = held_.find(key);
    return found ? *found : nullptr;
}

std::shared_ptr<const Archive> Archives::keep(const std::string& key, Archive archive) {
    const auto size = archive.data.size();
    auto kept = std::make_shared<const Archive>(std::move(archive));
    // an archive too big to keep is still handed back, it just is not remembered
    held_.keep(key, kept, size);
    return kept;
}

/**
 * the path inside is walked a segment at a time, because a tar names its files with
 * their directories and a zip may not name the directories at all: "assets" is not an
 * entry, "assets/icon.png" is. the first name that is an entry is either the file, when
 * nothing follows it, or an archive to open and keep walking in.
 */
Result<Blob> digArchive(std::shared_ptr<const Archive> archive, const std::string& key, std::string_view inside,
                        const ArchiveOptions& options) {
    size_t from = 0;
    while (true) {
        const auto slash = inside.find('/', from);
        const auto name = std::string(inside.substr(0, slash));

        if (const auto found = archive->index.entries.find(name); found != archive->index.entries.end()) {
            if (slash == std::string_view::npos) {
                auto data = readEntry(*archive, found->second, options.maxEntry);
                if (!data) return std::unexpected(data.error());
                return blobFrom(*std::move(data), name);
            }

            const auto childKey = key + "/" + name;
            auto child = options.archives ? options.archives->find(childKey) : nullptr;
            if (!child) {
                auto data = readEntry(*archive, found->second, options.maxEntry);
                if (!data) return std::unexpected(data.error());
                auto opened = openArchive(*std::move(data), name, options.maxEntry);
                if (!opened) return std::unexpected(opened.error());
                child = options.archives ? options.archives->keep(childKey, *std::move(opened))
                                         : std::make_shared<const Archive>(*std::move(opened));
            }
            return digArchive(std::move(child), childKey, inside.substr(slash + 1), options);
        }

        if (slash == std::string_view::npos) break;
        from = slash + 1;
    }

    // both halves came from the request, so repeating them tells the caller nothing
    // it did not already write
    const auto last = key.substr(key.find_last_of('/') + 1);
    return std::unexpected(Error{ErrorCode::NotFound, "no \"" + std::string(inside) + "\" inside " + last});
}

}
