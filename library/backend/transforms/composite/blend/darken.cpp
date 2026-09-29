#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = min("dst"_"rgb", "src"_"rgb")), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 */
template <>
BlendRes Blend<Compose::Darken>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) { return std::min(sc, dc); });
}

}
