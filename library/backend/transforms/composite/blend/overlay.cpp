#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = {(2 "dst"_"rgb" "src"_"rgb", "if " "dst"_"rgb" <= 0.5), (1 - 2(1 - "dst"_"rgb")(1 - "src"_"rgb"), "otherwise"):}), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 */
template <>
BlendRes Blend<Compose::Overlay>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) {
        return 2 * dc <= 1 ? 2 * sc * dc : 1 - 2 * (1 - sc) * (1 - dc);
    });
}

}
