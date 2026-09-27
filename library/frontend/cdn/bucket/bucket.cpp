#include <haio_cdn.hpp>

namespace Haio::Cdn {

/**
 * finds the bucket by name and hands the rest to Haio::Source, which knows how to
 * read every kind of origin. what is left here is only what the config says about
 * the cdn: which buckets exist, and whether a path may reach inside an archive.
 */
Task<Result<Blob>> fetchBucket(const Config& config, Source::Archives& archives, std::string bucketName, std::string path) {
    const auto& security = config.security;
    const auto it = config.buckets.find(bucketName);
    if (it == config.buckets.end()) {
        co_return std::unexpected(Error{ErrorCode::NotFound, "unknown bucket: " + bucketName});
    }

    const auto& bucket = it->second;

    if (auto inside = Source::splitArchivePath(path)) {
        if (!security.allowUnzip) {
            co_return std::unexpected(Error{ErrorCode::InvalidInput,
                                            "this path names a file inside an archive, and allow_unzip is off"});
        }
        co_return co_await Source::fetchInsideArchive(bucket, *std::move(inside), {security.maxUnzip, &archives});
    }

    co_return co_await Source::fetch(bucket, std::move(path));
}

}
