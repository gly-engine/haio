#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * a grey alone is how much of the colour shows, dark being all of it. only the alpha
 * is cut, so a half covered edge is the colour at half rather than a darker one.
 */
template <>
Result<Image<Color::RGBA8888>> Blend<Compose::Tint, Color::GRAY8>(Image<Color::GRAY8> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    Image<Color::RGBA8888> out{base.width, base.height,
                               std::vector<uint8_t>(static_cast<size_t>(base.width) * static_cast<size_t>(base.height) * 4)};
    for (int by = 0; by < base.height; by++) {
        for (int bx = 0; bx < base.width; bx++) {
            const auto at = static_cast<size_t>(by) * static_cast<size_t>(base.width) + static_cast<size_t>(bx);
            const auto* colour = colourAt(layer, bx - x, by - y);
            auto* pixel = out.data.data() + at * 4;
            pixel[3] = timesOf(colour[3], 255 - base.data[at]);
            if (pixel[3] != 0) std::copy_n(colour, 3, pixel);
        }
    }
    return out;
}

}
