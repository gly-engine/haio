#include <haio/internal/source/archive.hpp>
#include <haio/internal/source/storage.hpp>

namespace Haio::Source {

std::shared_ptr<Archives> makeArchives(size_t maxUsage) {
    return std::make_shared<Archives>(maxUsage);
}

/**
 * the outermost archive is fetched the ordinary way and kept, and everything inside
 * it is dug out of memory. the same picture twice never arrives here at all when
 * there is a cache in front: it answered before the origin was asked.
 */
Task<Result<Blob>> fetchInsideArchive(const Origin& origin, ArchivePath path, ArchiveOptions options) {
    const auto key = origin.name + "/" + path.archive;

    // the same net fetch() keeps: everything expected is a Result already
    try {
        auto archive = options.archives ? options.archives->find(key) : nullptr;
        if (!archive) {
            auto blob = co_await fetchAny(origin, path.archive);
            if (!blob) co_return std::unexpected(blob.error());

            auto opened = openArchive(std::move(blob->data), path.archive, options.maxEntry);
            if (!opened) co_return std::unexpected(opened.error());
            archive = options.archives ? options.archives->keep(key, *std::move(opened))
                                       : std::make_shared<const Archive>(*std::move(opened));
        }
        co_return digArchive(std::move(archive), key, path.inside, options);
    } catch (const std::exception& err) {
        co_return std::unexpected(Error{ErrorCode::Internal, err.what()});
    }
}

Result<Blob> readInsideArchive(std::vector<uint8_t> archive, std::string archiveName, std::string_view inside,
                               const ArchiveOptions& options) {
    auto opened = openArchive(std::move(archive), archiveName, options.maxEntry);
    if (!opened) return std::unexpected(opened.error());
    return digArchive(std::make_shared<const Archive>(*std::move(opened)), archiveName, inside, options);
}

}
