#include <haio_codec.hpp>
#include <haio_convert.hpp>
#include <haio_util.hpp>

namespace Haio::Codecs {
namespace {

/** one body for every colour the container can hold; the caller names which */
template <Color P>
Result<Image<P>> decodeKtx(const Blob& blob) {
    if (blob.data.size() < 68) HAIO_FAIL(InvalidInput, "invalid ktx header");
    if (Util::readU32LE(blob.data, 12) != 0x04030201) HAIO_FAIL(InvalidInput, "unsupported ktx endianness");
    if (Util::readU32LE(blob.data, 44) || Util::readU32LE(blob.data, 48)
        || Util::readU32LE(blob.data, 52) != 1 || Util::readU32LE(blob.data, 56) > 1) {
        HAIO_FAIL(InvalidInput, "unsupported ktx texture shape");
    }

    const Size size{static_cast<int>(Util::readU32LE(blob.data, 36)), static_cast<int>(Util::readU32LE(blob.data, 40))};
    const auto dataOff = static_cast<size_t>(64) + Util::readU32LE(blob.data, 60);
    if (!Util::hasBytes(blob.data, dataOff, 4)) HAIO_FAIL(InvalidInput, "truncated ktx container");

    HAIO_TRY(data, Util::slice(blob.data, dataOff + 4, Util::readU32LE(blob.data, dataOff)));

    HAIO_TRY(expected, sizeOf(P, size));
    if (data.size() != expected) HAIO_FAIL(InvalidInput, "invalid ktx payload size");

    return Image<P>{size.width, size.height, std::move(data)};
}

}

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::ETC1>> Decode<Format::KTX, Color::ETC1>(const Blob& blob) {
    return decodeKtx<Color::ETC1>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::RGB565>> Decode<Format::KTX, Color::RGB565>(const Blob& blob) {
    return decodeKtx<Color::RGB565>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Decode<Format::KTX, Color::RGBA8888>(const Blob& blob) {
    return decodeKtx<Color::RGBA8888>(blob);
}
/** @} */

}
