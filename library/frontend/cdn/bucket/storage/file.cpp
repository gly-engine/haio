#include <bucket/bucket.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>

namespace {

std::filesystem::path safeJoin(const std::filesystem::path& root, std::string_view rawPath) {
    std::filesystem::path rel(rawPath);
    if (rel.is_absolute()) rel = rel.relative_path();

    std::filesystem::path clean;
    for (const auto& part : rel) {
        if (part == "." || part.empty()) continue;
        if (part == "..") throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::InvalidInput, "path traversal is not allowed");
        // a leading "~" means nothing to the filesystem but everything to a shell, and
        // a name that reads as somebody's home directory has no business here
        if (part.native().front() == '~') {
            throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::InvalidInput, "\"~\" is not allowed in a path");
        }
        clean /= part;
    }
    return root / clean;
}

Haio::Blob readFileBlob(const Haio::Cdn::BucketConfig& bucket, std::string path) {
    const auto fullPath = safeJoin(bucket.root, path);
    std::ifstream in(fullPath, std::ios::binary);
    if (!in) {
        // the caller learns that it is not there and nothing else. where the bucket
        // lives, and therefore what else might be near it, is the server's business:
        // the resolved path goes to the log, which the operator can read and the
        // caller cannot
        std::cerr << "not found: " << fullPath.string() << "\n";
        throw Haio::Cdn::Bucket::Failure(Haio::ErrorCode::NotFound, "not found");
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto found = Haio::Detect(data);
    // the blob carries the path that was asked for, not the one on this disk: the
    // extension is the same either way, and nothing downstream should be able to
    // repeat where the bucket lives even by accident
    const auto format = found ? found.format : Haio::formatFromExtension(path);
    return Haio::Blob{format, found.color, std::string(Haio::contentTypeFor(format)), std::move(path), std::move(data)};
}

}

namespace Haio::Cdn::Bucket {

Blob fetchFile(const BucketConfig& bucket, std::string path) {
    return readFileBlob(bucket, std::move(path));
}

}
