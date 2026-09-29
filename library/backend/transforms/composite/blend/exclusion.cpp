#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "dst"_"rgb" + "src"_"rgb" - 2 "dst"_"rgb" "src"_"rgb"), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose exclusion -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose exclusion -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::Exclusion>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) { return sc + dc - 2 * sc * dc; });
}

}
