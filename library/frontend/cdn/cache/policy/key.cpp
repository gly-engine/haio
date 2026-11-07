#include <haio_cache.hpp>
#include <haio_util.hpp>
#include <haio/stage.hpp>

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

std::string cacheKey(std::string_view query) {
    return hexMd5(query);
}

std::vector<uint8_t> encodeEntry(const CacheEntry& entry) {
    std::vector<uint8_t> out;
    out.reserve(entry.data.size() + entry.contentType.size() + entry.filename.size() + 8);

    Util::appendU32LE(out, static_cast<uint32_t>(entry.contentType.size()));
    out.insert(out.end(), entry.contentType.begin(), entry.contentType.end());
    Util::appendU32LE(out, static_cast<uint32_t>(entry.filename.size()));
    out.insert(out.end(), entry.filename.begin(), entry.filename.end());
    out.push_back(static_cast<uint8_t>(entry.encoding));
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

    const auto encodingAt = nameAt + 4 + nameSize;
    if (raw.size() < encodingAt + 1 || raw[encodingAt] >= Stages::namesOf<BodyEncoding>().size()) return std::nullopt;

    CacheEntry entry;
    entry.contentType.assign(raw.begin() + 4, raw.begin() + 4 + typeSize);
    entry.filename.assign(raw.begin() + nameAt + 4, raw.begin() + nameAt + 4 + nameSize);
    entry.encoding = static_cast<BodyEncoding>(raw[encodingAt]);
    entry.data.assign(raw.begin() + encodingAt + 1, raw.end());
    return entry;
}

}
