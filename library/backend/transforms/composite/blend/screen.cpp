#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * "res"_"rgba" = 1 - (1 - "dst"_"rgba")(1 - "src"_"rgba")
 * @enduml
 */
template <>
BlendRes Blend<Compose::Screen>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) { return sc + dc - sc * dc; });
}

}
