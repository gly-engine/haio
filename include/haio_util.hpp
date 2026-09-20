#pragma once

#include "haio_codec.hpp"

#include <span>
#include <string_view>
#include <vector>

/**
 * byte level helpers shared by the codecs. every container haio reads or writes
 * stores its integers little endian, which the names say out loud: ktx even carries
 * an endianness marker that the decoder checks, so leaving it implicit was asking
 * for a silent bug on a big endian host.
 */
namespace Haio::Util {

bool matches(Bytes data, Bytes magic);
bool matches(Bytes data, std::string_view magic);

/** true when data holds len bytes starting at off, so a decoder checks once */
bool hasBytes(Bytes data, size_t off, size_t len);

/**
 * a four character code, the tag formats put at the front of a header. it is the
 * same operation as readU32LE over a literal, and it carries the suffix for the
 * same reason: riff and dds pack it little endian, iff and aiff pack it the other
 * way round, so "fourcc" alone does not say which.
 */
constexpr uint32_t fourccLE(char a, char b, char c, char d) {
    return static_cast<uint32_t>(static_cast<uint8_t>(a))
         | (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8)
         | (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16)
         | (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
}

void appendU32LE(std::vector<uint8_t>& out, uint32_t value);
void appendU64LE(std::vector<uint8_t>& out, uint64_t value);

/** reads zero when out of range; pair it with hasBytes where that matters */
uint32_t readU32LE(Bytes data, size_t off);

/** png and the netpbm family are big endian, unlike every gpu container here */
uint32_t readU32BE(Bytes data, size_t off);
uint64_t readU64LE(Bytes data, size_t off);

Result<std::vector<uint8_t>> slice(Bytes data, size_t off, size_t len);

/**
 * what the gpu containers need to agree on. these always spoke about colour rather
 * than about files; the old single enum just made it look otherwise.
 */
namespace GPU {

inline constexpr uint32_t GL_UNSIGNED_BYTE = 0x1401;
inline constexpr uint32_t GL_UNSIGNED_SHORT_5_6_5 = 0x8363;
inline constexpr uint32_t GL_RGB = 0x1907;
inline constexpr uint32_t GL_RGBA = 0x1908;
inline constexpr uint32_t GL_RGBA8 = 0x8058;
inline constexpr uint32_t GL_RGB565 = 0x8d62;
inline constexpr uint32_t GL_ETC1_RGB8_OES = 0x8d64;
inline constexpr uint32_t VK_FORMAT_R5G6B5_UNORM_PACK16 = 4;
inline constexpr uint32_t VK_FORMAT_R8G8B8A8_UNORM = 37;
inline constexpr uint32_t VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK = 147;

struct Profile {
    uint32_t glType;
    uint32_t glFormat;
    uint32_t glInternal;
    uint32_t glBase;
    uint32_t vkFormat;
    uint64_t pvrFormat;
    size_t size;
};

constexpr uint64_t pvrRaw(char a, char b, char c, char d, uint8_t ba, uint8_t bb, uint8_t bc, uint8_t bd) {
    return static_cast<uint64_t>(a) | (static_cast<uint64_t>(b) << 8) | (static_cast<uint64_t>(c) << 16)
         | (static_cast<uint64_t>(d) << 24) | (static_cast<uint64_t>(ba) << 32) | (static_cast<uint64_t>(bb) << 40)
         | (static_cast<uint64_t>(bc) << 48) | (static_cast<uint64_t>(bd) << 56);
}

/** etc1 rounds up to whole 4x4 blocks, so the stored size is not width times height */
constexpr size_t etc1Size(Size size) {
    return static_cast<size_t>((size.width + 3) / 4) * static_cast<size_t>((size.height + 3) / 4) * 8;
}

Result<size_t> payloadSize(Color color, Size size);
Result<Profile> profileFor(Color color, Size size);
uint32_t typeSize(const Profile& profile);

/** what colour a container header is describing */
Result<Color> colorFromGL(uint32_t glType, uint32_t glFormat, uint32_t glInternal);
Result<Color> colorFromVK(uint32_t format);
Result<Color> colorFromPVR(uint64_t format);

}

}
