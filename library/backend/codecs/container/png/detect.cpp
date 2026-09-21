#include <haio_codec.hpp>
#include <haio_util.hpp>

namespace {

constexpr std::array<uint8_t, 8> signature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};

/** IHDR is the first chunk and always 13 bytes, so the colour type sits at a fixed spot */
constexpr size_t colourTypeOffset = 25;
constexpr uint8_t colourGray = 0;
constexpr uint8_t colourRgb = 2;
constexpr uint8_t colourRgba = 6;

bool headerSays(Haio::Bytes data, uint8_t colourType) {
    return Haio::Util::matches(data, signature)
        && Haio::Util::hasBytes(data, colourTypeOffset, 1)
        && Haio::Util::matches(data.subspan(12, 4), "IHDR")
        && data[colourTypeOffset] == colourType;
}

}

namespace Haio::Codecs {

/** what a png holds when nobody asks for something else */
template <> struct DefaultColor<Format::PNG> { static constexpr Color value = Color::RGBA8888; };

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::PNG, Color::RGBA8888>(Bytes data) {
    return headerSays(data, colourRgba);
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::PNG, Color::RGB888>(Bytes data) {
    return headerSays(data, colourRgb);
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
/**
 * recognised on purpose with no decoder behind it: the error names the format.
 *
 * @todo palette (colour type 3) and gray+alpha (type 4) are not recognised at all,
 * because neither has a Color to name it. before this refactor wuffs expanded both
 * to rgba and they simply worked, so this is a regression until Color grows a
 * PALETTE8 and a GRAY_ALPHA -- and palette drags a colour table along with it.
 */
template <>
bool Detect<Format::PNG, Color::GRAY8>(Bytes data) {
    return headerSays(data, colourGray);
}
/** @} */

}
