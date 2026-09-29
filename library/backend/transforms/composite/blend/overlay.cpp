#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("result"_"rgb" = {(2 "dst"_"rgb" "src"_"rgb", "if " "dst"_"rgb" <= 0.5), (1 - 2(1 - "dst"_"rgb")(1 - "src"_"rgb"), "otherwise"):}), ("result"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 */
template <>
Result<Image<Color::RGBA8888>> Blend<Compose::Overlay>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    return blendSeparable(std::move(base), layer, x, y, [](double sc, double dc) {
        return 2 * dc <= 1 ? 2 * sc * dc : 1 - 2 * (1 - sc) * (1 - dc);
    });
}

}
