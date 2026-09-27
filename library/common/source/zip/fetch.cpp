#include <haio/internal/source/storage.hpp>
#include <haio/internal/source/zip.hpp>

namespace Haio::Source {

std::shared_ptr<ZipArchives> makeZipArchives(size_t maxUsage) {
    return std::make_shared<ZipArchives>(maxUsage);
}

bool namesZipEntry(std::string_view path) {
    return splitZipPath(path).has_value();
}

namespace {

/**
 * the archive, from the ones already kept or fetched the ordinary way and then kept,
 * so a second picture out of the same zip costs an inflate and no fetch.
 */
Task<Result<std::shared_ptr<const ZipArchives::Archive>>> archiveFor(const Origin& origin, const ZipPath& path, ZipArchives* archives) {
    const auto key = origin.name + "/" + path.archive;
    if (auto kept = archives ? archives->find(key) : nullptr) co_return kept;

    auto blob = co_await fetchAny(origin, path.archive);
    if (!blob) co_return std::unexpected(blob.error());

    auto index = readZipIndex(blob->data);
    if (!index) co_return std::unexpected(index.error());

    if (archives) co_return archives->keep(key, std::move(blob->data), *std::move(index));
    co_return std::make_shared<const ZipArchives::Archive>(ZipArchives::Archive{std::move(blob->data), *std::move(index)});
}

Task<Result<Blob>> readInsideZip(const Origin& origin, const ZipPath& path, ZipOptions options) {
    const auto archive = co_await archiveFor(origin, path, options.archives);
    if (!archive) co_return std::unexpected(archive.error());

    const auto found = (*archive)->index.entries.find(path.inside);
    if (found == (*archive)->index.entries.end()) {
        // both halves came from the request, so repeating them tells the caller
        // nothing it did not already write
        co_return std::unexpected(Error{ErrorCode::NotFound, "no \"" + path.inside + "\" inside " + path.archive});
    }

    auto data = readZipEntry((*archive)->data, found->second, options.maxEntry);
    if (!data) co_return std::unexpected(data.error());
    co_return blobFrom(*std::move(data), path.inside);
}

}

Task<Result<Blob>> fetchInsideZip(const Origin& origin, std::string path, ZipOptions options) {
    const auto zipped = splitZipPath(path);
    if (!zipped) co_return std::unexpected(Error{ErrorCode::InvalidInput, "no archive in " + path});

    // the same net fetch() keeps: everything expected is a Result already
    try {
        co_return co_await readInsideZip(origin, *zipped, options);
    } catch (const std::exception& err) {
        co_return std::unexpected(Error{ErrorCode::Internal, err.what()});
    }
}

}
