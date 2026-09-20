#include <haio_codec.hpp>
#include <haio_convert.hpp>
#include <haio_util.hpp>

namespace Haio::Codecs {
namespace {

/** one body for every colour the container can hold; the caller names which */
template <Color P>
Result<Image<P>> decodePvr(const Blob& blob) {
    if (blob.data.size() < 52) HAIO_FAIL(InvalidInput, "invalid pvr header");
    if (Util::readU32LE(blob.data, 32) != 1 || Util::readU32LE(blob.data, 36) != 1
        || Util::readU32LE(blob.data, 40) != 1 || Util::readU32LE(blob.data, 44) != 1) {
        HAIO_FAIL(InvalidInput, "unsupported pvr texture shape");
    }

    const Size size{static_cast<int>(Util::readU32LE(blob.data, 28)), static_cast<int>(Util::readU32LE(blob.data, 24))};
    HAIO_TRY(want, sizeOf(P, size));
    HAIO_TRY(data, Util::slice(blob.data, static_cast<size_t>(52) + Util::readU32LE(blob.data, 48), want));

    HAIO_TRY(expected, sizeOf(P, size));
    if (data.size() != expected) HAIO_FAIL(InvalidInput, "invalid pvr payload size");

    return Image<P>{size.width, size.height, std::move(data)};
}

}

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::ETC1>> Decode<Format::PVR, Color::ETC1>(const Blob& blob) {
    return decodePvr<Color::ETC1>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::RGB565>> Decode<Format::PVR, Color::RGB565>(const Blob& blob) {
    return decodePvr<Color::RGB565>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Decode<Format::PVR, Color::RGBA8888>(const Blob& blob) {
    return decodePvr<Color::RGBA8888>(blob);
}
/** @} */

}
