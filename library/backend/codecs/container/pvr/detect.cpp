#include <haio_codec.hpp>
#include <haio_util.hpp>

namespace {

/** the container is only half the answer: the header also says which colour it holds */
bool holds(Haio::Bytes data, Haio::Color wanted) {
    constexpr std::array<uint8_t, 4> magic = {'P', 'V', 'R', 0x03};
    if (!Haio::Util::matches(data, magic)) return false;

    const auto colour = Haio::Util::GPU::colorFromPVR(Haio::Util::readU64LE(data, 8));
    return colour && *colour == wanted;
}

}

namespace Haio::Codecs {

/**
 * @addtogroup detect
 * @{
 */

template <>
bool Detect<Format::PVR, Color::ETC1>(Bytes data) {
    return holds(data, Color::ETC1);
}

template <>
bool Detect<Format::PVR, Color::ETC2>(Bytes data) {
    return holds(data, Color::ETC2);
}

template <>
bool Detect<Format::PVR, Color::ETC2A>(Bytes data) {
    return holds(data, Color::ETC2A);
}

template <>
bool Detect<Format::PVR, Color::EAC_R11>(Bytes data) {
    return holds(data, Color::EAC_R11);
}

template <>
bool Detect<Format::PVR, Color::EAC_RG11>(Bytes data) {
    return holds(data, Color::EAC_RG11);
}

template <>
bool Detect<Format::PVR, Color::RGB565>(Bytes data) {
    return holds(data, Color::RGB565);
}

template <>
bool Detect<Format::PVR, Color::RGBA8888>(Bytes data) {
    return holds(data, Color::RGBA8888);
}

/** @} */

}
