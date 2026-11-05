#include <haio_codec.hpp>
#include <haio_util.hpp>

namespace {

/** the container is only half the answer: the header also says which colour it holds */
bool holds(Haio::Bytes data, Haio::Color wanted) {
    constexpr std::array<uint8_t, 12> magic = {0xab, 'K', 'T', 'X', ' ', '1', '1', 0xbb, '\r', '\n', 0x1a, '\n'};
    if (!Haio::Util::matches(data, magic)) return false;

    const auto colour = Haio::Util::GPU::colorFromGL(Haio::Util::readU32LE(data, 16),
                                                     Haio::Util::readU32LE(data, 24),
                                                     Haio::Util::readU32LE(data, 28));
    return colour && *colour == wanted;
}

}

namespace Haio::Codecs {

/**
 * @addtogroup detect
 * @{
 */

template <>
bool Detect<Format::KTX, Color::ETC1>(Bytes data) {
    return holds(data, Color::ETC1);
}

template <>
bool Detect<Format::KTX, Color::ETC2>(Bytes data) {
    return holds(data, Color::ETC2);
}

template <>
bool Detect<Format::KTX, Color::ETC2A>(Bytes data) {
    return holds(data, Color::ETC2A);
}

template <>
bool Detect<Format::KTX, Color::EAC_R11>(Bytes data) {
    return holds(data, Color::EAC_R11);
}

template <>
bool Detect<Format::KTX, Color::EAC_RG11>(Bytes data) {
    return holds(data, Color::EAC_RG11);
}

template <>
bool Detect<Format::KTX, Color::RGB565>(Bytes data) {
    return holds(data, Color::RGB565);
}

template <>
bool Detect<Format::KTX, Color::RGBA8888>(Bytes data) {
    return holds(data, Color::RGBA8888);
}

/** @} */

}
