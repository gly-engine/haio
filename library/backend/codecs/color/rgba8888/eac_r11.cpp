#include <haio/internal/codecs/etc.hpp>
#include <haio_convert.hpp>

#include <ProcessRGB.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** the red channel alone, eleven bits of it a pixel; the rest of the picture is dropped */
template <>
Result<void> Move<Color::RGBA8888, Color::EAC_R11>(Bytes src, std::span<uint8_t> dst, Size size) {
    return Etc::compress(src, dst, size, Color::EAC_R11, CompressEacR);
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::EAC_R11>> Convert<Color::RGBA8888, Color::EAC_R11>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::EAC_R11>(std::move(src));
}
/** @} */

}
