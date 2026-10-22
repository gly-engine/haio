#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "src"_"rgb" - "dst"_"rgb"), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose minusdst -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose minusdst -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::MinusDst>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) { return sc - dc; });
}

}
