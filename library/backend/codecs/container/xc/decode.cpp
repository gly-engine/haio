#include <haio_codec.hpp>
#include <haio/codecs/xc.hpp>
#include <haio_string.hpp>
#include <haio_util.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

namespace Haio::Codecs {

/**
 * a canvas of one colour. the blob holds the words after "xc:", not a file, and the
 * size is the one setting it reads; a side is capped so a typo cannot ask for more
 * memory than the machine has.
 */
template <>
Result<Image<Color::RGBA8888>> Decode<Format::XC, Color::RGBA8888>(const Blob& blob, const Settings& settings) {
    constexpr int largest = 16384;

    const auto* wanted = settingNamed(settings, xcSize.name());
    const std::string_view spelled = wanted ? std::string_view{wanted->value} : xcSize.fallback;
    if (const auto why = Stages::refusal(xcSize, spelled)) HAIO_FAIL(InvalidInput, *why);

    Size size;
    if (!String::tryGetSize(spelled, size)) HAIO_FAIL(InvalidInput, "invalid argument for option `-size': " + std::string(spelled));
    if (size.width > largest || size.height > largest) {
        HAIO_FAIL(InvalidInput, "width or height exceeds limit of " + std::to_string(largest) + " "
                                    + Stages::quoted(spelled));
    }

    const std::string_view text{reinterpret_cast<const char*>(blob.data.data()), blob.data.size()};
    // imagemagick draws xc: with nothing after it in white
    const auto colour = Util::GetColorFromName(text.empty() ? std::string_view{"white"} : text);
    if (!colour) HAIO_FAIL(InvalidInput, "unrecognized color " + Stages::quoted(text));
    const uint8_t rgba[4] = {static_cast<uint8_t>(*colour >> 16), static_cast<uint8_t>(*colour >> 8),
                             static_cast<uint8_t>(*colour), static_cast<uint8_t>(*colour >> 24)};

    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    std::vector<uint8_t> data(pixels * 4);
    for (size_t at = 0; at < pixels; at++) std::copy_n(rgba, 4, data.data() + at * 4);
    return Image<Color::RGBA8888>{size.width, size.height, std::move(data)};
}

}
