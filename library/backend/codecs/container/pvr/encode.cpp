#include <haio_codec.hpp>
#include <haio_convert.hpp>
#include <haio_util.hpp>

namespace Haio::Codecs {
namespace {

template <Color P>
Result<Blob> encodePvr(Image<P> img) {
    const Size size{img.width, img.height};
    HAIO_TRY(expected, sizeOf(P, size));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "pvr payload does not match its size");

    HAIO_TRY(profile, Util::GPU::profileFor(P, size));
    std::vector<uint8_t> out;
    for (auto v : {0x03525650u, 0u}) Util::appendU32LE(out, v);
    Util::appendU64LE(out, profile.pvrFormat);
    for (auto v : {0u, 0u, static_cast<uint32_t>(size.height), static_cast<uint32_t>(size.width),
                   1u, 1u, 1u, 1u, 0u}) {
        Util::appendU32LE(out, v);
    }
    out.insert(out.end(), img.data.begin(), img.data.end());

    return Blob{Format::PVR, P, "image/x-pvr", {}, std::move(out)};
}

}

/**
 * @addtogroup encode
 * @{
 */
template <>
Result<Blob> Encode<Format::PVR, Color::ETC1>(Image<Color::ETC1> img) {
    return encodePvr<Color::ETC1>(std::move(img));
}
/** @} */

/**
 * @addtogroup encode
 * @{
 */
template <>
Result<Blob> Encode<Format::PVR, Color::RGB565>(Image<Color::RGB565> img) {
    return encodePvr<Color::RGB565>(std::move(img));
}
/** @} */

/**
 * @addtogroup encode
 * @{
 */
template <>
Result<Blob> Encode<Format::PVR, Color::RGBA8888>(Image<Color::RGBA8888> img) {
    return encodePvr<Color::RGBA8888>(std::move(img));
}
/** @} */

}
