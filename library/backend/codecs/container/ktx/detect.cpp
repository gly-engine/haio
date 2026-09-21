#include <haio_codec.hpp>
#include <haio_util.hpp>

#include <haio_util.hpp>

namespace {

/** the container is only half the answer: the header also says which colour it holds */
bool isContainer(Haio::Bytes data) {


    constexpr std::array<uint8_t, 12> magic = {0xab, 'K', 'T', 'X', ' ', '1', '1', 0xbb, '\r', '\n', 0x1a, '\n'};
    return Haio::Util::matches(data, magic);
}

}

namespace Haio::Codecs {

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::KTX, Color::ETC1>(Bytes data) {
    if (!isContainer(data)) return false;
    const auto colour = Util::GPU::colorFromGL(Util::readU32LE(data, 16), Util::readU32LE(data, 24), Util::readU32LE(data, 28));
    return colour && *colour == Color::ETC1;
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::KTX, Color::RGB565>(Bytes data) {
    if (!isContainer(data)) return false;
    const auto colour = Util::GPU::colorFromGL(Util::readU32LE(data, 16), Util::readU32LE(data, 24), Util::readU32LE(data, 28));
    return colour && *colour == Color::RGB565;
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::KTX, Color::RGBA8888>(Bytes data) {
    if (!isContainer(data)) return false;
    const auto colour = Util::GPU::colorFromGL(Util::readU32LE(data, 16), Util::readU32LE(data, 24), Util::readU32LE(data, 28));
    return colour && *colour == Color::RGBA8888;
}
/** @} */

}
