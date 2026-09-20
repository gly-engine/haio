#include <haio_codec.hpp>
#include <haio_convert.hpp>
#include <haio_util.hpp>

namespace Haio::Codecs {
namespace {

template <Color P>
Result<Blob> encodeKtx2(Image<P> img) {
    const Size size{img.width, img.height};
    HAIO_TRY(expected, sizeOf(P, size));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "ktx2 payload does not match its size");

    HAIO_TRY(profile, Util::GPU::profileFor(P, size));
    std::vector<uint8_t> out = {0xab, 0x4b, 0x54, 0x58, 0x20, 0x32, 0x30, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
    for (auto v : {profile.vkFormat, Util::GPU::typeSize(profile), static_cast<uint32_t>(size.width),
                   static_cast<uint32_t>(size.height), 0u, 0u, 1u, 1u, 0u, 0u, 0u, 0u, 0u}) {
        Util::appendU32LE(out, v);
    }
    Util::appendU64LE(out, 0);
    Util::appendU64LE(out, 0);
    Util::appendU64LE(out, 104);
    Util::appendU64LE(out, img.data.size());
    Util::appendU64LE(out, img.data.size());
    out.insert(out.end(), img.data.begin(), img.data.end());

    return Blob{Format::KTX2, P, "image/ktx2", {}, std::move(out)};
}

}

/**
 * @addtogroup encode
 * @{
 */
template <>
Result<Blob> Encode<Format::KTX2, Color::ETC1>(Image<Color::ETC1> img) {
    return encodeKtx2<Color::ETC1>(std::move(img));
}
/** @} */

/**
 * @addtogroup encode
 * @{
 */
template <>
Result<Blob> Encode<Format::KTX2, Color::RGB565>(Image<Color::RGB565> img) {
    return encodeKtx2<Color::RGB565>(std::move(img));
}
/** @} */

/**
 * @addtogroup encode
 * @{
 */
template <>
Result<Blob> Encode<Format::KTX2, Color::RGBA8888>(Image<Color::RGBA8888> img) {
    return encodeKtx2<Color::RGBA8888>(std::move(img));
}
/** @} */

}
