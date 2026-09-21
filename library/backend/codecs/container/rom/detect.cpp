#include <haio_codec.hpp>

namespace Haio::Codecs {

/** a rom holds pattern data, and pattern data is what haio can draw */
template <> struct DefaultColor<Format::ROM> { static constexpr Color value = Color::CHR_NES; };

/**
 * @addtogroup detect
 * @{
 */

/** the ines header, which every nes rom on the internet starts with */
template <>
bool Detect<Format::ROM, Color::CHR_NES>(Bytes data) {
    return data.size() >= 16 && data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 0x1A;
}

/** @} */

}
