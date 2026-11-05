#include <haio/internal/codecs/etc.hpp>
#include <haio_convert.hpp>

#include <ProcessRGB.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** lossy, sixteen bytes a block: an eac alpha block and then the colour */
template <>
Result<void> Move<Color::RGBA8888, Color::ETC2A>(Bytes src, std::span<uint8_t> dst, Size size) {
    return Etc::compress(src, dst, size, Color::ETC2A,
                         [](const uint32_t* from, uint64_t* to, uint32_t blocks, size_t width) {
                             CompressEtc2Rgba(from, to, blocks, width, true);
                         });
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::ETC2A>> Convert<Color::RGBA8888, Color::ETC2A>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::ETC2A>(std::move(src));
}
/** @} */

}
