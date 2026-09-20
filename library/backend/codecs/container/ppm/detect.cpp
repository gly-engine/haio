#include <haio_codec.hpp>

namespace {

/** the magic is two bytes and a separator: "P6\n", never "P6x" */
bool isNetpbm(Haio::Bytes data, char low, char high) {
    return data.size() >= 3 && data[0] == 'P' && data[1] >= static_cast<uint8_t>(low)
        && data[1] <= static_cast<uint8_t>(high)
        && (data[2] == '\n' || data[2] == '\r' || data[2] == ' ' || data[2] == '\t' || data[2] == '#');
}

}

namespace Haio::Codecs {

/** what a bare "ppm" means when nobody says which colour */
template <> struct DefaultColor<Format::PPM> { static constexpr Color value = Color::RGB888; };

/**
 * @addtogroup detect
 * @{
 */
/** P3 is ascii pixmap and P6 binary pixmap: three samples a pixel */
template <>
bool Detect<Format::PPM, Color::RGB888>(Bytes data) {
    return isNetpbm(data, '3', '3') || isNetpbm(data, '6', '6');
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
/**
 * the rest of the family is one sample a pixel: P1 and P4 bitmaps, P2 and P5
 * graymaps. they answer as grey so the decoder that gets them is the right one.
 */
template <>
bool Detect<Format::PPM, Color::GRAY8>(Bytes data) {
    return isNetpbm(data, '1', '2') || isNetpbm(data, '4', '5');
}
/** @} */

}
