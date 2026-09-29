#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * @startuml{math}
 * {: ("res"_"rgb" = ("src"_"rgb" "src"_"a" + "dst"_"rgb" "dst"_"a" (1 - "src"_"a")) / "res"_"a"), ("res"_"a" = "src"_"a" + "dst"_"a"(1 - "src"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose over -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose over -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::Over>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendWith(std::move(dst), src, x, y,
                     [](double sc, double sa, double dc, double da) { return sa >= 1 ? sc : sc * sa + dc * da * (1 - sa); },
                     unionOf);
}

}
