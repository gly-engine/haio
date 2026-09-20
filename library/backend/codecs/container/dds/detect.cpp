#include <haio_codec.hpp>
#include <haio_util.hpp>

#include <haio_util.hpp>

namespace {

/** dds says its colour through the pixel format mask rather than a single field */
Haio::Result<Haio::Color> ddsColor(Haio::Bytes data) {
    const auto bits = Haio::Util::readU32LE(data, 88);
    const auto r = Haio::Util::readU32LE(data, 92);
    const auto g = Haio::Util::readU32LE(data, 96);
    const auto b = Haio::Util::readU32LE(data, 100);
    const auto a = Haio::Util::readU32LE(data, 104);
    if (bits == 16 && r == 0xf800 && g == 0x07e0 && b == 0x001f && a == 0) return Haio::Color::RGB565;
    if (bits == 32 && r == 0x000000ff && g == 0x0000ff00 && b == 0x00ff0000 && a == 0xff000000) return Haio::Color::RGBA8888;
    return std::unexpected(Haio::Error{Haio::ErrorCode::UnsupportedFormat, "unsupported dds pixel format"});
}

/** the container is only half the answer: the header also says which colour it holds */
bool isContainer(Haio::Bytes data) {


    // "DDS " alone is a weak signal, so the header size that follows it is checked too
    if (!Haio::Util::matches(data, "DDS ") || data.size() < 8) return false;
    const uint32_t headerSize = static_cast<uint32_t>(data[4]) | (static_cast<uint32_t>(data[5]) << 8)
                              | (static_cast<uint32_t>(data[6]) << 16) | (static_cast<uint32_t>(data[7]) << 24);
    return headerSize == 124;
}

}

namespace Haio::Codecs {

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::DDS, Color::RGB565>(Bytes data) {
    if (!isContainer(data)) return false;
    const auto colour = ddsColor(data);
    return colour && *colour == Color::RGB565;
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::DDS, Color::RGBA8888>(Bytes data) {
    if (!isContainer(data)) return false;
    const auto colour = ddsColor(data);
    return colour && *colour == Color::RGBA8888;
}
/** @} */

}
