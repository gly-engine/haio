#include <haio_codec.hpp>
#include <haio_util.hpp>

#include <haio_util.hpp>

namespace {

/** the container is only half the answer: the header also says which colour it holds */
bool isContainer(Haio::Bytes data) {


    constexpr std::array<uint8_t, 12> magic = {0xab, 'K', 'T', 'X', ' ', '2', '0', 0xbb, '\r', '\n', 0x1a, '\n'};
    return Haio::Util::matches(data, magic);
}

}

namespace Haio::Codecs {

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::KTX2, Color::ETC1>(Bytes data) {
    if (!isContainer(data)) return false;
    const auto colour = Util::GPU::colorFromVK(Util::readU32LE(data, 12));
    return colour && *colour == Color::ETC1;
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::KTX2, Color::RGB565>(Bytes data) {
    if (!isContainer(data)) return false;
    const auto colour = Util::GPU::colorFromVK(Util::readU32LE(data, 12));
    return colour && *colour == Color::RGB565;
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::KTX2, Color::RGBA8888>(Bytes data) {
    if (!isContainer(data)) return false;
    const auto colour = Util::GPU::colorFromVK(Util::readU32LE(data, 12));
    return colour && *colour == Color::RGBA8888;
}
/** @} */

}
