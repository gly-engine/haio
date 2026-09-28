#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/** screen on the colours, multiply on the alphas */
template <>
Result<Image<Color::RGBA8888>> Blend<Compose::Tint>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    for (int by = 0; by < base.height; by++) {
        for (int bx = 0; bx < base.width; bx++) {
            auto* shape = base.data.data() + (static_cast<size_t>(by) * static_cast<size_t>(base.width) + static_cast<size_t>(bx)) * 4;
            const auto* colour = colourAt(layer, bx - x, by - y);
            for (int c = 0; c < 3; c++) shape[c] = screenOf(shape[c], colour[c]);
            shape[3] = timesOf(shape[3], colour[3]);
        }
    }
    return base;
}

}
