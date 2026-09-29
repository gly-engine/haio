#pragma once

#include "haio.hpp"
#include "haio_task.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * where the bytes of a picture come from, for anything that needs one: the cdn's
 * buckets and the convert command line alike.
 *
 * none of this does any i/o of its own beyond reading a file. it decides what to ask
 * for -- which url, which headers, what signature -- hands that to the platform, and
 * decides what the answer was. which is why it compiles wherever the platform does,
 * a browser included.
 */
namespace Haio::Source {

/**
 * a place bytes can be read from, described entirely by the scheme of its url:
 *
 *   file://relative/dir     file:///absolute/dir
 *   http://host/prefix      https://host/prefix      s3://host/bucket
 *   https://\*               open, the path names the host
 *   //\*                     open, the path names the scheme and the host
 *
 * the cdn calls one a bucket and reads it from its config; the command line makes
 * one out of the http url it was given, and takes no s3 ones yet.
 */
struct Origin {
    /** what errors call it; the cdn uses the bucket's name */
    std::string name;
    std::string url;

    // derived from the url by resolveOrigin, so a bad one fails before any request
    std::string scheme;
    bool open = false;
    std::filesystem::path root;

    /**
     * s3 only, and only when the request is signed: it is part of the signature, not
     * of the address, so nothing about the connection can supply it.
     */
    std::string region;

    /**
     * s3 only, and the reason a config file deserves careful permissions. empty reads
     * the environment instead, and empty there leaves the request unsigned, which is
     * what a public bucket wants.
     */
    std::string accessKey;
    std::string secretKey;
    std::string sessionToken;
};

/**
 * a whole file, sized once and read in one go, or nothing when it cannot be opened
 * or read. growing a vector a byte at a time instead copied a 32mb png once for every
 * doubling it took.
 */
std::optional<std::vector<uint8_t>> readFile(const std::filesystem::path& path);

/**
 * bytes named as a picture. the bytes win over what anybody claims, because the other
 * two are what lie: gam creatives carry no extension and are often mislabelled. so a
 * format the bytes cannot tell falls back to the content type a server sent, and then
 * to the extension of the path.
 */
Blob blobFrom(std::vector<uint8_t> data, std::string path, std::string_view contentType = {});

/** fills in what the url implies, or throws saying why the url cannot be used */
void resolveOrigin(Origin& origin);

/** one file out of an origin, with its format worked out from the bytes first */
Task<Result<Blob>> fetch(const Origin& origin, std::string path);

/** true for the urls fetchUri takes, so a caller can tell one from a local path */
bool isRemoteUri(std::string_view text);

/**
 * a single http or https url, as the command line is handed one; the whole url is
 * the file. s3 is left to the cdn for now, where a bucket has somewhere to write its
 * region and its keys.
 */
Task<Result<Blob>> fetchUri(std::string uri);

/**
 * "foo.ipk/data.tar.gz/assets/icon.png" split where the file ends and the archive
 * begins: the first segment named like an archive (.zip, .ipk, .deb, .tar, .tgz,
 * .gz) is the file, and the rest is read out of it, opening whatever archives it
 * holds on the way.
 */
struct ArchivePath {
    std::string archive;
    std::string inside;
};
std::optional<ArchivePath> splitArchivePath(std::string_view path);

/**
 * the archives read out of an origin, kept so a second picture out of the same one
 * costs no fetch and no inflate. bounded by what they hold altogether.
 */
class Archives;
std::shared_ptr<Archives> makeArchives(size_t maxUsage);

struct ArchiveOptions {
    /** the largest one extracted file may be, and one undone gzip */
    size_t maxEntry = 64u << 20;
    Archives* archives = nullptr;
};

/** one file out of an archive the origin holds */
Task<Result<Blob>> fetchInsideArchive(const Origin& origin, ArchivePath path, ArchiveOptions options);

/**
 * the same, out of an archive already in hand, which is how the command line reads
 * one it found on a disk or behind a url. nothing is kept unless options says where.
 */
Result<Blob> readInsideArchive(std::vector<uint8_t> archive, std::string archiveName, std::string_view inside,
                               const ArchiveOptions& options);

}
