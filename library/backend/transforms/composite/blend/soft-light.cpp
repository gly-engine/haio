#include <haio/transforms/composite.hpp>

#include <cmath>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = {("dst"_"rgb" - (1 - 2 "src"_"rgb") "dst"_"rgb" (1 - "dst"_"rgb"), "if " "src"_"rgb" <= 0.5), ("dst"_"rgb" + (2 "src"_"rgb" - 1)(D("dst"_"rgb") - "dst"_"rgb"), "otherwise"):}), (D(x) = {(((16x - 12)x + 4)x, "if " x <= 0.25), (sqrt(x), "otherwise"):}), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose softlight -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose softlight -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::SoftLight>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) {
        if (2 * sc <= 1) return dc - (1 - 2 * sc) * dc * (1 - dc);
        const double d = dc <= 0.25 ? ((16 * dc - 12) * dc + 4) * dc : std::sqrt(dc);
        return dc + (2 * sc - 1) * (d - dc);
    });
}

}
