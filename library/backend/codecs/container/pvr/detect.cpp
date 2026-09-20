#include <haio_codec.hpp>
#include <haio_util.hpp>

#include <haio_util.hpp>

namespace {

/** the container is only half the answer: the header also says which colour it holds */
bool isContainer(Haio::Bytes data) {


    constexpr std::array<uint8_t, 4> magic = {'P', 'V', 'R', 0x03};
    return Haio::Util::matches(data, magic);
}

}

namespace Haio::Codecs {

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::PVR, Color::ETC1>(Bytes data) {
    if (!isContainer(data)) return false;
    const auto colour = Util::GPU::colorFromPVR(Util::readU64LE(data, 8));
    return colour && *colour == Color::ETC1;
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::PVR, Color::RGB565>(Bytes data) {
    if (!isContainer(data)) return false;
    const auto colour = Util::GPU::colorFromPVR(Util::readU64LE(data, 8));
    return colour && *colour == Color::RGB565;
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::PVR, Color::RGBA8888>(Bytes data) {
    if (!isContainer(data)) return false;
    const auto colour = Util::GPU::colorFromPVR(Util::readU64LE(data, 8));
    return colour && *colour == Color::RGBA8888;
}
/** @} */

}
