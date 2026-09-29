#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the same as rgba8888, the grey standing for all three channels.
 *
 * @startuml{math}
 * {: ("result"_"rgb" = 1 - (1 - "dst"_"g")(1 - "src"_"rgb")), ("result"_"a" = "dst"_"a" "src"_"a") :}
 * @enduml
 */
template <>
Result<Image<Color::RGBA8888>> Blend<Compose::Tint, Color::GRAYALPHA88>(Image<Color::GRAYALPHA88> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    Image<Color::RGBA8888> out{base.width, base.height,
                               std::vector<uint8_t>(static_cast<size_t>(base.width) * static_cast<size_t>(base.height) * 4)};
    for (int by = 0; by < base.height; by++) {
        for (int bx = 0; bx < base.width; bx++) {
            const auto at = static_cast<size_t>(by) * static_cast<size_t>(base.width) + static_cast<size_t>(bx);
            const int grey = base.data[at * 2];
            const int alpha = base.data[at * 2 + 1];
            const auto* colour = colourAt(layer, bx - x, by - y);
            auto* pixel = out.data.data() + at * 4;
            for (int c = 0; c < 3; c++) pixel[c] = screenOf(grey, colour[c]);
            pixel[3] = timesOf(alpha, colour[3]);
        }
    }
    return out;
}

}
