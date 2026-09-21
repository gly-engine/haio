#include <haio_codec.hpp>
#include <haio_convert.hpp>
#include <haio_util.hpp>

namespace Haio::Codecs {
namespace {

/** one body for every colour the container can hold; the caller names which */
template <Color P>
Result<Image<P>> decodeKtx2(const Blob& blob) {
    if (blob.data.size() < 104) HAIO_FAIL(InvalidInput, "invalid ktx2 header");
    if (Util::readU32LE(blob.data, 28) || Util::readU32LE(blob.data, 32) || Util::readU32LE(blob.data, 36) != 1
        || Util::readU32LE(blob.data, 40) != 1 || Util::readU32LE(blob.data, 44)) {
        HAIO_FAIL(InvalidInput, "unsupported ktx2 texture shape");
    }

    const Size size{static_cast<int>(Util::readU32LE(blob.data, 20)), static_cast<int>(Util::readU32LE(blob.data, 24))};
    HAIO_TRY(data, Util::slice(blob.data, static_cast<size_t>(Util::readU64LE(blob.data, 80)),
                                          static_cast<size_t>(Util::readU64LE(blob.data, 88))));

    HAIO_TRY(expected, sizeOf(P, size));
    if (data.size() != expected) HAIO_FAIL(InvalidInput, "invalid ktx2 payload size");

    return Image<P>{size.width, size.height, std::move(data)};
}

}

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::ETC1>> Decode<Format::KTX2, Color::ETC1>(const Blob& blob) {
    return decodeKtx2<Color::ETC1>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::RGB565>> Decode<Format::KTX2, Color::RGB565>(const Blob& blob) {
    return decodeKtx2<Color::RGB565>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Decode<Format::KTX2, Color::RGBA8888>(const Blob& blob) {
    return decodeKtx2<Color::RGBA8888>(blob);
}
/** @} */

}
