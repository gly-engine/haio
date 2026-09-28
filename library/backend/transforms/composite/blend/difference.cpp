#include <haio/transforms/composite.hpp>

#include <cmath>

namespace Haio::Transforms {

template <>
Result<Image<Color::RGBA8888>> Blend<Compose::Difference>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    return blendSeparable(std::move(base), layer, x, y, [](double sc, double dc) { return std::abs(sc - dc); });
}

}
