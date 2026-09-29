#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = {(1, "if " "dst"_"rgb" + "src"_"rgb" >= 1), ("dst"_"rgb" / (1 - "src"_"rgb"), "otherwise"):}), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 */
template <>
BlendRes Blend<Compose::ColorDodge>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) {
        // as imagemagick 6 asks it, so white over black is white
        if (sc + dc >= 1) return 1.0;
        return dc / (1 - sc);
    });
}

}
