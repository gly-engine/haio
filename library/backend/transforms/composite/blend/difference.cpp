#include <haio/transforms/composite.hpp>

#include <cmath>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = |"dst"_"rgb" - "src"_"rgb"|), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose difference -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose difference -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::Difference>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) { return std::abs(sc - dc); });
}

}
