#include <haio_codec.hpp>
#include <haio_util.hpp>

namespace {

constexpr std::array<uint8_t, 6> signature87a = {
    'G', 'I', 'F', '8', '7', 'a'
};

constexpr std::array<uint8_t, 6> signature89a = {
    'G', 'I', 'F', '8', '9', 'a'
};

bool headerSays(Haio::Bytes data) {
    return Haio::Util::matches(data, signature87a)
        || Haio::Util::matches(data, signature89a);
}

}

namespace Haio::Codecs {

/** what a gif holds when nobody asks for something else */
template <>
struct DefaultColor<Format::GIF> {
    static constexpr Color value = Color::RGBA8888;
};

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::GIF, Color::RGBA8888>(Bytes data) {
    return headerSays(data);
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
/**
 * GIF has a palette rather than an RGB colour type, but the decoder expands
 * it directly to RGB888.
 */
template <>
bool Detect<Format::GIF, Color::RGB888>(Bytes data) {
    return headerSays(data);
}
/** @} */

}
