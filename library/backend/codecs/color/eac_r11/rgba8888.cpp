#include <haio/internal/codecs/etc.hpp>
#include <haio_convert.hpp>

#include <Decode.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** back into red alone: green and blue come back black, because that is what was stored */
template <>
Result<void> Move<Color::EAC_R11, Color::RGBA8888>(Bytes src, std::span<uint8_t> dst, Size size) {
    return Etc::decompress(src, dst, size, Color::EAC_R11,
                           [](const uint64_t* from, uint32_t* to, int32_t width, int32_t height) {
                               DecodeR(from, to, width, height);
                           },
                           true);
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Convert<Color::EAC_R11, Color::RGBA8888>(Image<Color::EAC_R11> src) {
    return convertVia<Color::EAC_R11, Color::RGBA8888>(std::move(src));
}
/** @} */

}
