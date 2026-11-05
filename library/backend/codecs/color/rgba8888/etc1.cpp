#include <haio/internal/codecs/etc.hpp>
#include <haio_convert.hpp>

#include <ProcessRGB.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** lossy: four by four blocks, dithered, and the dimensions round up to whole blocks */
template <>
Result<void> Move<Color::RGBA8888, Color::ETC1>(Bytes src, std::span<uint8_t> dst, Size size) {
    return Etc::compress(src, dst, size, Color::ETC1, CompressEtc1RgbDither);
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::ETC1>> Convert<Color::RGBA8888, Color::ETC1>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::ETC1>(std::move(src));
}
/** @} */

}
