#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

template <>
Result<Image<Color::RGBA8888>> Blend<Compose::ColorDodge>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    return blendSeparable(std::move(base), layer, x, y, [](double sc, double dc) {
        // as imagemagick 6 asks it, so white over black is white
        if (sc + dc >= 1) return 1.0;
        return dc / (1 - sc);
    });
}

}
