#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

template <>
Result<Image<Color::RGBA8888>> Composite<Color::RGBA8888>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, Gravity gravity, int x, int y) {
    const auto [left, top] = placeOf({base.width, base.height}, {layer.width, layer.height}, gravity, x, y);
    return composeOver(std::move(base), layer, left, top);
}

}
