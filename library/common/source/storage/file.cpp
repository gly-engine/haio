#include <haio/internal/source/storage.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {

Haio::Result<std::filesystem::path> safeJoin(const std::filesystem::path& root, std::string_view rawPath) {
    std::filesystem::path rel(rawPath);
    if (rel.is_absolute()) rel = rel.relative_path();

    std::filesystem::path clean;
    for (const auto& part : rel) {
        if (part == "." || part.empty()) continue;
        if (part == "..") HAIO_FAIL(InvalidInput, "path traversal is not allowed");
        // a leading "~" means nothing to the filesystem but everything to a shell, and
        // a name that reads as somebody's home directory has no business here
        if (part.native().front() == '~') HAIO_FAIL(InvalidInput, "\"~\" is not allowed in a path");
        clean /= part;
    }
    return root / clean;
}

}

namespace Haio::Source {

std::optional<std::vector<uint8_t>> readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;

    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

    std::vector<uint8_t> data(size);
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size))) return std::nullopt;
    return data;
}

Result<Blob> fetchFile(const Origin& origin, std::string path) {
    const auto fullPath = safeJoin(origin.root, path);
    if (!fullPath) return std::unexpected(fullPath.error());

    auto data = readFile(*fullPath);
    if (!data) {
        // the caller learns that it is not there and nothing else. where the bucket
        // lives, and therefore what else might be near it, is the server's business:
        // the resolved path goes to the log, which the operator can read and the
        // caller cannot
        std::cerr << "not found: " << fullPath->string() << "\n";
        HAIO_FAIL(NotFound, "not found");
    }

    // the blob carries the path that was asked for, not the one on this disk: the
    // extension is the same either way, and nothing downstream should be able to
    // repeat where the bucket lives even by accident
    return blobFrom(*std::move(data), std::move(path));
}

}
