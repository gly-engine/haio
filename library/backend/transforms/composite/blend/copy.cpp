#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * src as it is where it reaches, and dst where it does not.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "src"_"rgb"), ("res"_"a" = "src"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copy -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copy -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::Copy>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        (void)d;
        return s;
    });
}

}
