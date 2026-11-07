#include <haio/internal/codecs/etc.hpp>
#include <haio_convert.hpp>

#include <Decode.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** etc1 keeps no alpha, so every pixel comes back opaque */
template <>
Result<void> Move<Color::ETC1, Color::RGBA8888>(Bytes src, std::span<uint8_t> dst, Size size) {
    return Etc::decompress(src, dst, size, Color::ETC1, DecodeRGB, true);
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Convert<Color::ETC1, Color::RGBA8888>(Image<Color::ETC1> src) {
    return convertVia<Color::ETC1, Color::RGBA8888>(std::move(src));
}
/** @} */

}
