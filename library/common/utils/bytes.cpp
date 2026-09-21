#include <haio_util.hpp>

namespace Haio::Util {

bool hasBytes(Bytes data, size_t off, size_t len) {
    return off <= data.size() && len <= data.size() - off;
}

void appendU32LE(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 24));
}

void appendU64LE(std::vector<uint8_t>& out, uint64_t value) {
    appendU32LE(out, static_cast<uint32_t>(value));
    appendU32LE(out, static_cast<uint32_t>(value >> 32));
}

uint32_t readU32LE(Bytes data, size_t off) {
    if (!hasBytes(data, off, 4)) return 0;
    return static_cast<uint32_t>(data[off + 0])
         | (static_cast<uint32_t>(data[off + 1]) << 8)
         | (static_cast<uint32_t>(data[off + 2]) << 16)
         | (static_cast<uint32_t>(data[off + 3]) << 24);
}

uint32_t readU32BE(Bytes data, size_t off) {
    if (!hasBytes(data, off, 4)) return 0;
    return (static_cast<uint32_t>(data[off + 0]) << 24)
         | (static_cast<uint32_t>(data[off + 1]) << 16)
         | (static_cast<uint32_t>(data[off + 2]) << 8)
         | static_cast<uint32_t>(data[off + 3]);
}

uint64_t readU64LE(Bytes data, size_t off) {
    return static_cast<uint64_t>(readU32LE(data, off))
         | (static_cast<uint64_t>(readU32LE(data, off + 4)) << 32);
}

Result<std::vector<uint8_t>> slice(Bytes data, size_t off, size_t len) {
    if (!hasBytes(data, off, len)) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "truncated payload"});
    }
    return std::vector<uint8_t>{data.begin() + static_cast<std::ptrdiff_t>(off),
                                data.begin() + static_cast<std::ptrdiff_t>(off + len)};
}

}
