#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * "result"_"rgba" = 1 - (1 - "dst"_"rgba")(1 - "src"_"rgba")
 * @enduml
 */
template <>
Result<Image<Color::RGBA8888>> Blend<Compose::Screen>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    return blendSeparable(std::move(base), layer, x, y, [](double sc, double dc) { return sc + dc - sc * dc; });
}

}
