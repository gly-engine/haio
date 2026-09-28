#include <haio/transforms/composite.hpp>

#include <cmath>

namespace Haio::Transforms {

/** the w3c formula */
template <>
Result<Image<Color::RGBA8888>> Blend<Compose::SoftLight>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    return blendSeparable(std::move(base), layer, x, y, [](double sc, double dc) {
        if (2 * sc <= 1) return dc - (1 - 2 * sc) * dc * (1 - dc);
        const double d = dc <= 0.25 ? ((16 * dc - 12) * dc + 4) * dc : std::sqrt(dc);
        return dc + (2 * sc - 1) * (d - dc);
    });
}

}
