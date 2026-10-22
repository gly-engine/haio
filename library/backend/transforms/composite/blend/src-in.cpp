#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * src where dst is, and like imagemagick nothing where src does not reach.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "src"_"rgb"), ("res"_"a" = "src"_"a" "dst"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose srcin -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose srcin -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::SrcIn>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Cleared,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        return {s[0], s[1], s[2], s[3] * d[3]};
    });
}

}
