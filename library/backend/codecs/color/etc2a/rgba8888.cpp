#include <haio/internal/codecs/etc.hpp>
#include <haio_convert.hpp>

#include <Decode.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** the one etc that carries an alpha, so the decoder's is kept rather than overwritten */
template <>
Result<void> Move<Color::ETC2A, Color::RGBA8888>(Bytes src, std::span<uint8_t> dst, Size size) {
    return Etc::decompress(src, dst, size, Color::ETC2A, DecodeRGBA, false);
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Convert<Color::ETC2A, Color::RGBA8888>(Image<Color::ETC2A> src) {
    return convertVia<Color::ETC2A, Color::RGBA8888>(std::move(src));
}
/** @} */

}
