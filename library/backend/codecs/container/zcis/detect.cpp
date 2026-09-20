#include <haio_codec.hpp>
#include <haio_util.hpp>

namespace Haio::Codecs {

/** every layer inside is a ppm, so rgba is what the encoder takes in */
template <> struct DefaultColor<Format::ZCIS> { static constexpr Color value = Color::RGBA8888; };

/**
 * @addtogroup detect
 * @{
 */
/**
 * @todo zcis has no decoder at all, so a .zcis round trip is impossible. it also
 * cannot say which colour is inside without reading the member names.
 */
template <>
bool Detect<Format::ZCIS, Color::RGBA8888>(Bytes data) {
    return Util::matches(data, "!<arch>\n");
}
/** @} */

}
