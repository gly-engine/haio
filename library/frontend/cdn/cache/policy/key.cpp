#include <haio_cache.hpp>
#include <haio_util.hpp>

#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/md5.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string hexMd5(std::string_view text) {
    wc_Md5 state;
    if (wc_InitMd5(&state) != 0) throw std::runtime_error("cannot start md5");

    wc_Md5Update(&state, reinterpret_cast<const uint8_t*>(text.data()), static_cast<word32>(text.size()));
    uint8_t digest[WC_MD5_DIGEST_SIZE];
    wc_Md5Final(&state, digest);

    static constexpr char hex[] = "0123456789abcdef";
    std::string out(WC_MD5_DIGEST_SIZE * 2, '0');
    for (size_t at = 0; at < WC_MD5_DIGEST_SIZE; at++) {
        out[at * 2] = hex[digest[at] >> 4];
        out[at * 2 + 1] = hex[digest[at] & 0x0F];
    }
    return out;
}

}

namespace Haio::Cdn {

std::string cacheKey(std::string_view bucket, std::string_view path, std::string_view query) {
    std::string normalised;
    normalised.reserve(bucket.size() + path.size() + query.size() + 2);
    normalised += bucket;
    normalised += '\n';
    normalised += path;
    normalised += '\n';
    normalised += query;
    return hexMd5(normalised);
}

std::vector<uint8_t> encodeEntry(const CacheEntry& entry) {
    std::vector<uint8_t> out;
    out.reserve(entry.data.size() + entry.contentType.size() + entry.filename.size() + 8);

    Util::appendU32LE(out, static_cast<uint32_t>(entry.contentType.size()));
    out.insert(out.end(), entry.contentType.begin(), entry.contentType.end());
    Util::appendU32LE(out, static_cast<uint32_t>(entry.filename.size()));
    out.insert(out.end(), entry.filename.begin(), entry.filename.end());
    out.insert(out.end(), entry.data.begin(), entry.data.end());
    return out;
}

/** a truncated or corrupt entry reads as a miss rather than as a bad response */
std::optional<CacheEntry> decodeEntry(std::span<const uint8_t> raw) {
    if (raw.size() < 4) return std::nullopt;

    const auto typeSize = Util::readU32LE(raw, 0);
    if (raw.size() < 4 + typeSize + 4) return std::nullopt;

    const auto nameAt = 4 + typeSize;
    const auto nameSize = Util::readU32LE(raw, nameAt);
    if (raw.size() < nameAt + 4 + nameSize) return std::nullopt;

    const auto dataAt = nameAt + 4 + nameSize;
    CacheEntry entry;
    entry.contentType.assign(raw.begin() + 4, raw.begin() + 4 + typeSize);
    entry.filename.assign(raw.begin() + nameAt + 4, raw.begin() + nameAt + 4 + nameSize);
    entry.data.assign(raw.begin() + dataAt, raw.end());
    return entry;
}

}
