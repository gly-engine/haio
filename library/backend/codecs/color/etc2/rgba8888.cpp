#include <haio/internal/codecs/etc.hpp>
#include <haio_convert.hpp>

#include <Decode.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** the same decoder etc1 uses: an etc1 block is an etc2 block. no alpha either way */
template <>
Result<void> Move<Color::ETC2, Color::RGBA8888>(Bytes src, std::span<uint8_t> dst, Size size) {
    return Etc::decompress(src, dst, size, Color::ETC2, DecodeRGB, true);
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Convert<Color::ETC2, Color::RGBA8888>(Image<Color::ETC2> src) {
    return convertVia<Color::ETC2, Color::RGBA8888>(std::move(src));
}
/** @} */

}
