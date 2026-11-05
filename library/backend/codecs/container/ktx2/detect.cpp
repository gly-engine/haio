#include <haio_codec.hpp>
#include <haio_util.hpp>

namespace {

/**
 * the container is only half the answer: the header also says which colour it holds.
 *
 * vulkan has no etc1 format, so a ktx2 never answers etc1: one written from etc1
 * says etc2 and is read back as etc2, which decodes to the same pixels.
 */
bool holds(Haio::Bytes data, Haio::Color wanted) {
    constexpr std::array<uint8_t, 12> magic = {0xab, 'K', 'T', 'X', ' ', '2', '0', 0xbb, '\r', '\n', 0x1a, '\n'};
    if (!Haio::Util::matches(data, magic)) return false;

    const auto colour = Haio::Util::GPU::colorFromVK(Haio::Util::readU32LE(data, 12));
    return colour && *colour == wanted;
}

}

namespace Haio::Codecs {

/**
 * @addtogroup detect
 * @{
 */

template <>
bool Detect<Format::KTX2, Color::ETC1>(Bytes data) {
    return holds(data, Color::ETC1);
}

template <>
bool Detect<Format::KTX2, Color::ETC2>(Bytes data) {
    return holds(data, Color::ETC2);
}

template <>
bool Detect<Format::KTX2, Color::ETC2A>(Bytes data) {
    return holds(data, Color::ETC2A);
}

template <>
bool Detect<Format::KTX2, Color::EAC_R11>(Bytes data) {
    return holds(data, Color::EAC_R11);
}

template <>
bool Detect<Format::KTX2, Color::EAC_RG11>(Bytes data) {
    return holds(data, Color::EAC_RG11);
}

template <>
bool Detect<Format::KTX2, Color::RGB565>(Bytes data) {
    return holds(data, Color::RGB565);
}

template <>
bool Detect<Format::KTX2, Color::RGBA8888>(Bytes data) {
    return holds(data, Color::RGBA8888);
}

/** @} */

}
