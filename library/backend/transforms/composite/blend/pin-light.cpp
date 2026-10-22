#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = {(2 "src"_"rgb" - 1, "if " "dst"_"rgb" < 2 "src"_"rgb" - 1), (2 "src"_"rgb", "if " "dst"_"rgb" > 2 "src"_"rgb"), ("dst"_"rgb", "otherwise"):}), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose pinlight -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose pinlight -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::PinLight>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) {
        if (dc < 2 * sc - 1) return 2 * sc - 1;
        if (dc > 2 * sc) return 2 * sc;
        return dc;
    });
}

}
