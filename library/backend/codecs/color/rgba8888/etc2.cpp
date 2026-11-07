#include <haio/internal/codecs/etc.hpp>
#include <haio_convert.hpp>

#include <ProcessRGB.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** lossy, the same eight bytes a block as etc1 with the three etc2 modes to choose from */
template <>
Result<void> Move<Color::RGBA8888, Color::ETC2>(Bytes src, std::span<uint8_t> dst, Size size) {
    return Etc::compress(src, dst, size, Color::ETC2,
                         [](const uint32_t* from, uint64_t* to, uint32_t blocks, size_t width) {
                             CompressEtc2Rgb(from, to, blocks, width, true);
                         });
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::ETC2>> Convert<Color::RGBA8888, Color::ETC2>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::ETC2>(std::move(src));
}
/** @} */

}
