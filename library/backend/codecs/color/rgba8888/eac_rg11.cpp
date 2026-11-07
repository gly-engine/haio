#include <haio/internal/codecs/etc.hpp>
#include <haio_convert.hpp>

#include <ProcessRGB.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** red and green, eleven bits each; blue and alpha are dropped */
template <>
Result<void> Move<Color::RGBA8888, Color::EAC_RG11>(Bytes src, std::span<uint8_t> dst, Size size) {
    return Etc::compress(src, dst, size, Color::EAC_RG11, CompressEacRg);
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::EAC_RG11>> Convert<Color::RGBA8888, Color::EAC_RG11>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::EAC_RG11>(std::move(src));
}
/** @} */

}
