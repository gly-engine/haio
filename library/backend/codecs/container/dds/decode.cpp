#include <haio_codec.hpp>
#include <haio_convert.hpp>
#include <haio_util.hpp>

namespace Haio::Codecs {
namespace {

/** one body for every colour the container can hold; the caller names which */
template <Color P>
Result<Image<P>> decodeDds(const Blob& blob) {
    if (blob.data.size() < 128 || Util::readU32LE(blob.data, 4) != 124 || Util::readU32LE(blob.data, 76) != 32) {
        HAIO_FAIL(InvalidInput, "invalid dds header");
    }

    const Size size{static_cast<int>(Util::readU32LE(blob.data, 16)), static_cast<int>(Util::readU32LE(blob.data, 12))};
    HAIO_TRY(want, sizeOf(P, size));
    HAIO_TRY(data, Util::slice(blob.data, 128, want));

    HAIO_TRY(expected, sizeOf(P, size));
    if (data.size() != expected) HAIO_FAIL(InvalidInput, "invalid dds payload size");

    return Image<P>{size.width, size.height, std::move(data)};
}

}

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::RGB565>> Decode<Format::DDS, Color::RGB565>(const Blob& blob) {
    return decodeDds<Color::RGB565>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Decode<Format::DDS, Color::RGBA8888>(const Blob& blob) {
    return decodeDds<Color::RGBA8888>(blob);
}
/** @} */

}
