#include <haio_convert.hpp>

#include <algorithm>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** nothing becomes clear: every byte zero, the alpha with them */
template <>
Result<void> Move<Color::NIL, Color::RGBA8888>(Bytes, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (dst.size() < pixels * 4) {
        HAIO_FAIL(InvalidInput, "nil to rgba8888 got a buffer that is too small");
    }
    std::fill_n(dst.begin(), pixels * 4, uint8_t{0});
    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Convert<Color::NIL, Color::RGBA8888>(Image<Color::NIL> src) {
    return convertVia<Color::NIL, Color::RGBA8888>(std::move(src));
}
/** @} */

}
