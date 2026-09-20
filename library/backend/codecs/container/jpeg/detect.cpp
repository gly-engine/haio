#include <haio_codec.hpp>

namespace Haio::Codecs {

/** a jpeg stores yuv, so that is what it holds even though nothing can read it yet */
template <> struct DefaultColor<Format::JPEG> { static constexpr Color value = Color::YUV420; };

/**
 * @addtogroup detect
 * @{
 */

/**
 * start of image, then the first marker. every jpeg opens FF D8 FF whatever follows,
 * be it a jfif header, an exif block or a bare quantisation table.
 *
 * @todo detect only: there is no jpeg decoder, so this answers "jpeg yuv420" and then
 * Decode refuses. that refusal naming the format is the point, and it is what let
 * describeForeignMagic go away.
 */
template <>
bool Detect<Format::JPEG, Color::YUV420>(Bytes data) {
    return data.size() >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
}

/** @} */

}
