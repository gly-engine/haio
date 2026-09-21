#include <bucket/bucket.hpp>

#include <boost/asio/use_awaitable.hpp>

namespace asio = boost::asio;

namespace Haio::Cdn::Bucket {

/**
 * reads one file out of an archive the bucket holds.
 *
 * the archive is fetched the ordinary way and then kept, so a second picture out of
 * the same zip costs an inflate and no fetch. the same picture twice never arrives
 * here at all: the response cache answered before the bucket was asked.
 */
asio::awaitable<Blob> fetchInsideZip(const BucketConfig& bucket, const ZipPath& path,
                                     const SecurityConfig& security, ZipArchives& archives) {
    const auto key = bucket.name + "/" + path.archive;

    auto archive = archives.find(key);
    if (!archive) {
        auto blob = bucket.scheme == "file" ? fetchFile(bucket, path.archive)
                  : bucket.scheme == "s3"     ? co_await fetchS3(bucket, path.archive)
                                              : co_await fetchHttp(bucket, path.archive);

        auto index = readZipIndex(blob.data);
        if (!index) throw Failure(index.error().code, index.error().message);

        archive = archives.keep(key, std::move(blob.data), *std::move(index));
    }

    const auto found = archive->index.entries.find(path.inside);
    if (found == archive->index.entries.end()) {
        // both halves came from the request, so repeating them tells the caller
        // nothing it did not already write
        throw Failure(ErrorCode::NotFound, "no \"" + path.inside + "\" inside " + path.archive);
    }

    auto data = readZipEntry(archive->data, found->second, security.maxUnzip);
    if (!data) throw Failure(data.error().code, data.error().message);

    const auto detected = Haio::Detect(*data);
    const auto format = detected ? detected.format : Haio::formatFromExtension(path.inside);
    co_return Blob{format, detected.color, std::string(Haio::contentTypeFor(format)), path.inside, *std::move(data)};
}

}
