#pragma once

#include "haio_codec.hpp"

#include <optional>
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
 * a colour by the name imagemagick gives it -- "tomato", "gray50", "#ff8800",
 * "transparent" -- with imagemagick's value, packed 0xAARRGGBB the way a palette packs one. nothing when the
 * text names no colour. canvases draw with it, and so will gradients.
 */
std::optional<uint32_t> GetColorFromName(std::string_view name);

/**
 * what the gpu containers need to agree on. these always spoke about colour rather
 * than about files; the old single enum just made it look otherwise.
 */
/**
 * ordered dithering, which is a threshold that depends on where a pixel is rather
 * than on what its neighbours became.
 *
 * it lives here and not in the palette code because the matrix is not about colour
 * at all: the same thresholds order a gradient, pick which pixels of a mask survive
 * a downscale, or break up a banded sky. a caller that needs one of those should not
 * have to reach into a quantiser to find it.
 */
namespace Dither {

/**
 * the threshold at x,y of a side by side matrix, from 0 to side*side-1.
 *
 * the matrix is defined by doubling: each step turns one cell into four, keeping the
 * order top left, bottom right, top right, bottom left. that is why the side has to
 * be a power of two, and why the same function answers for a four colour palette and
 * a two hundred and fifty six colour one.
 */
constexpr int bayer(int side, int x, int y) {
    if (side <= 1) return 0;

    const auto half = side / 2;
    // the order the quadrants are filled in, which is what makes the pattern spread
    // rather than clump
    constexpr int quadrant[4] = {0, 2, 3, 1};
    const auto which = (y >= half ? 2 : 0) + (x >= half ? 1 : 0);

    return 4 * bayer(half, x % half, y % half) + quadrant[which];
}

/** how wide a matrix has to be to hold that many levels, rounded up to a power of two */
constexpr int bayerSideFor(int levels) {
    int side = 2;
    while (side * side < levels && side < 16) side *= 2;
    return side;
}

}

namespace GPU {

inline constexpr uint32_t GL_UNSIGNED_BYTE = 0x1401;
inline constexpr uint32_t GL_UNSIGNED_SHORT_5_6_5 = 0x8363;
inline constexpr uint32_t GL_RED = 0x1903;
inline constexpr uint32_t GL_RGB = 0x1907;
inline constexpr uint32_t GL_RGBA = 0x1908;
inline constexpr uint32_t GL_RG = 0x8227;
inline constexpr uint32_t GL_RGBA8 = 0x8058;
inline constexpr uint32_t GL_RGB565 = 0x8d62;
inline constexpr uint32_t GL_ETC1_RGB8_OES = 0x8d64;
inline constexpr uint32_t GL_COMPRESSED_R11_EAC = 0x9270;
inline constexpr uint32_t GL_COMPRESSED_RG11_EAC = 0x9272;
inline constexpr uint32_t GL_COMPRESSED_RGB8_ETC2 = 0x9274;
inline constexpr uint32_t GL_COMPRESSED_RGBA8_ETC2_EAC = 0x9278;
inline constexpr uint32_t VK_FORMAT_R5G6B5_UNORM_PACK16 = 4;
inline constexpr uint32_t VK_FORMAT_R8G8B8A8_UNORM = 37;
inline constexpr uint32_t VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK = 147;
inline constexpr uint32_t VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK = 151;
inline constexpr uint32_t VK_FORMAT_EAC_R11_UNORM_BLOCK = 153;
inline constexpr uint32_t VK_FORMAT_EAC_R11G11_UNORM_BLOCK = 155;

/**
 * what a pvr3 header calls a compressed payload: a number rather than four characters
 * with their widths, which is what pvrRaw below builds for the uncompressed ones.
 */
inline constexpr uint64_t PVR_ETC1 = 6;
inline constexpr uint64_t PVR_ETC2_RGB = 22;
inline constexpr uint64_t PVR_ETC2_RGBA = 23;
inline constexpr uint64_t PVR_EAC_R11 = 25;
inline constexpr uint64_t PVR_EAC_RG11 = 26;

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

/**
 * how many bytes one four by four block takes, or zero when the colour is not packed
 * into blocks at all. colour alone is eight; an alpha block in front of it, or a
 * second channel beside it, doubles that.
 */
constexpr size_t blockBytes(Color color) {
    switch (color) {
        case Color::ETC1:     return 8;
        case Color::ETC2:     return 8;
        case Color::EAC_R11:  return 8;
        case Color::ETC2A:    return 16;
        case Color::EAC_RG11: return 16;
        default:              return 0;
    }
}

/** a blocked colour rounds up to whole blocks, so its size is not width times height */
constexpr size_t blockedSize(Color color, Size size) {
    return static_cast<size_t>((size.width + 3) / 4) * static_cast<size_t>((size.height + 3) / 4)
         * blockBytes(color);
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
