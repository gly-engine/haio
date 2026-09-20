#include <haio_codec.hpp>
#include <haio_convert.hpp>
#include <haio_util.hpp>

namespace Haio::Codecs {
namespace {

template <Color P>
Result<Blob> encodeDds(Image<P> img) {
    const Size size{img.width, img.height};
    HAIO_TRY(expected, sizeOf(P, size));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "dds payload does not match its size");

    const auto pitch = static_cast<uint32_t>(size.width * (P == Color::RGB565 ? 2 : 4));
    const auto flags = P == Color::RGB565 ? 0x40u : 0x41u;
    const auto bits = P == Color::RGB565 ? 16u : 32u;
    const auto r = P == Color::RGB565 ? 0xf800u : 0x000000ffu;
    const auto g = P == Color::RGB565 ? 0x07e0u : 0x0000ff00u;
    const auto b = P == Color::RGB565 ? 0x001fu : 0x00ff0000u;
    const auto a = P == Color::RGB565 ? 0u : 0xff000000u;

    std::vector<uint8_t> out;
    for (auto v : {Util::fourccLE('D', 'D', 'S', ' '), 124u, 0x100fu, static_cast<uint32_t>(size.height),
                   static_cast<uint32_t>(size.width), pitch, 0u, 0u}) {
        Util::appendU32LE(out, v);
    }
    for (int i = 0; i < 11; i++) Util::appendU32LE(out, 0);
    for (auto v : {32u, flags, 0u, bits, r, g, b, a, 0x1000u, 0u, 0u, 0u, 0u}) Util::appendU32LE(out, v);
    out.insert(out.end(), img.data.begin(), img.data.end());

    return Blob{Format::DDS, P, "image/vnd-ms.dds", {}, std::move(out)};
}

}

/**
 * @addtogroup encode
 * @{
 */
template <>
Result<Blob> Encode<Format::DDS, Color::RGB565>(Image<Color::RGB565> img) {
    return encodeDds<Color::RGB565>(std::move(img));
}
/** @} */

/**
 * @addtogroup encode
 * @{
 */
template <>
Result<Blob> Encode<Format::DDS, Color::RGBA8888>(Image<Color::RGBA8888> img) {
    return encodeDds<Color::RGBA8888>(std::move(img));
}
/** @} */

}
