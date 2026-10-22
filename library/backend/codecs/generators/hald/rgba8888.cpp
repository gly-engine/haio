#include <haio_codec.hpp>
#include <haio/codecs/generators/hald.hpp>

#include <charconv>

namespace Haio::Codecs {

/**
 * an identity hald colour lookup table, the one imagemagick's hald: draws: level n
 * has n*n steps of each channel, laid out red fastest and blue slowest across a
 * square n*n*n pixels on a side. hald: alone is level 8, 512 by 512.
 */
template <>
Result<Image<Color::RGBA8888>> Generate<Brush::Hald, Color::RGBA8888>(std::string_view words, const Settings&) {
    constexpr int deepest = 16;

    int level = 8;
    if (!words.empty()) {
        const auto [end, problem] = std::from_chars(words.data(), words.data() + words.size(), level);
        if (problem != std::errc{} || end != words.data() + words.size() || level < 2 || level > deepest) {
            HAIO_FAIL(InvalidInput, "invalid hald level, which is 2 to " + std::to_string(deepest) + " " + Stages::quoted(words));
        }
    }

    const int steps = level * level;
    const int side = steps * level;
    auto image = Canvas::blank({side, side});

    // rounded down, as imagemagick does: hald:3 steps by 31, 63, 95 and so on
    const auto step = [&](int i) { return static_cast<uint32_t>(i * 255 / (steps - 1)); };
    for (int at = 0; at < side * side; at++) {
        const int r = at % steps;
        const int g = (at / steps) % steps;
        const int b = at / (steps * steps);
        Canvas::put(image, at % side, at / side, 0xFF000000u | (step(r) << 16) | (step(g) << 8) | step(b));
    }
    return image;
}

}
