#include <haio_codec.hpp>
#include <haio_util.hpp>

#include <string_view>

namespace Haio::Codecs {

/** every layer inside is a ppm, so rgba is what the encoder takes in */
template <> struct DefaultColor<Format::ZCIS> { static constexpr Color value = Color::RGBA8888; };

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::ZCIS, Color::RGBA8888>(Bytes data) {
    if (!Util::matches(data, "!<arch>\n") || data.size() < 8 + 60) return false;
    const std::string_view header(reinterpret_cast<const char*>(data.data() + 8), 60);
    auto name = header.substr(0, 16);
    while (!name.empty() && (name.back() == ' ' || name.back() == '/')) name.remove_suffix(1);
    return name == "0000000000.txt" && header.substr(58) == "`\n";
}
/** @} */

}
