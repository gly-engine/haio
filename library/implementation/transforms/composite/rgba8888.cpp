#include <haio/implementation/transforms/composite.hpp>
#include <haio/generated/transforms.hpp>

namespace Haio::Implementation::Transforms {

Result<Image<Color::RGBA8888>> Transform<Operation::Composite, Color::RGBA8888>::operator()(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, Compose compose, Transform<>::Gravity gravity, int x, int y) const {
    const auto [left, top] = placeOf({base.width, base.height}, {layer.width, layer.height}, gravity, x, y);
    return Blend<>{}(compose, std::move(base), layer, left, top);
}

}
