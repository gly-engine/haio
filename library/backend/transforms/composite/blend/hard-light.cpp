#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

template <>
Result<Image<Color::RGBA8888>> Blend<Compose::HardLight>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    return blendSeparable(std::move(base), layer, x, y, [](double sc, double dc) {
        return 2 * sc <= 1 ? 2 * sc * dc : 1 - 2 * (1 - sc) * (1 - dc);
    });
}

}
