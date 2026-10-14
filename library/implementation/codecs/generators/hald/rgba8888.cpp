#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/implementation/codecs/generators/hald.hpp>

#include <charconv>

namespace Haio::Implementation::Codecs {

/**
 * an identity hald colour lookup table, the one imagemagick's hald: draws: level n
 * has n*n steps of each channel, laid out red fastest and blue slowest across a
 * square n*n*n pixels on a side. hald: alone is level 8, 512 by 512.
 */
CodecRes<Color::RGBA8888> Generate<Brush::Hald, Color::RGBA8888>::operator()(std::string_view words, const Settings&) const {
    constexpr int deepest = 16;

    int level = 8;
    if (!words.empty()) {
        const auto [end, problem] = std::from_chars(words.data(), words.data() + words.size(), level);
        if (problem != std::errc{} || end != words.data() + words.size() || level < 2 || level > deepest) {
            HAIO_FAIL(InvalidInput, "invalid hald level, which is 2 to " + std::to_string(deepest) + " " + Util::quoted(words));
        }
    }

    const int steps = level * level;
    const int side = steps * level;
    auto image = Util::getCanvasBlank({side, side});

    // rounded down, as imagemagick does: hald:3 steps by 31, 63, 95 and so on
    const auto step = [&](int i) { return static_cast<uint32_t>(i * 255 / (steps - 1)); };
    for (int at = 0; at < side * side; at++) {
        const int r = at % steps;
        const int g = (at / steps) % steps;
        const int b = at / (steps * steps);
        Util::setCanvasPixel(image, at % side, at / side, 0xFF000000u | (step(r) << 16) | (step(g) << 8) | step(b));
    }
    return image;
}

}
