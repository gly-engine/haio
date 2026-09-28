#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

template <>
Result<Image<Color::RGBA8888>> Blend<Compose::ColorBurn>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    return blendSeparable(std::move(base), layer, x, y, [](double sc, double dc) {
        if (dc >= 1) return 1.0;
        if (sc <= 0) return 0.0;
        return 1 - std::min(1.0, (1 - dc) / sc);
    });
}

}
