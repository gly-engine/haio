#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = {(1, "if " "src"_"rgb" = 1), (1 - (1 - "dst"_"rgb") / (2 "src"_"rgb"), "if " 2 "src"_"rgb" <= 1), ("dst"_"rgb" / (2 (1 - "src"_"rgb")), "otherwise"):}), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose vividlight -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose vividlight -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::VividLight>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) {
        if (sc == 1) return 1.0;
        if (2 * sc <= 1) return 1 - (1 - dc) * reciprocalOf(2 * sc);
        return dc * reciprocalOf(2 * (1 - sc));
    });
}

}
