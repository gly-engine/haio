#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * @startuml{math}
 * {: ("res"_"rgb" = min(1, ("dst"_"rgb" "dst"_"a" + "src"_"rgb" "src"_"a") / "res"_"a")), ("res"_"a" = min(1, "dst"_"a" + "src"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose plus -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose plus -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::Plus>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendWith(std::move(dst), src, x, y,
                     [](double sc, double sa, double dc, double da) { return sc * sa + dc * da; },
                     [](double sa, double da) { return sa + da; });
}

}
