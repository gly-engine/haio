#include <haio/transforms/composite.hpp>
#include "hcl.hpp"

namespace Haio::Transforms {

/**
 * dst's colour with src's luma, in imagemagick 6's hue, chroma and luma; Haio::Transforms::Hcl::blend
 * has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "hcl"("dst hue, dst chroma, src luma")), ("res"_"a" = max("src"_"a", "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose luminize -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose luminize -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::Luminize>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        return Hcl::blend(s, d, [](Hcl::Split from, Hcl::Split into) {
            into.luma = from.luma;
            return into;
        });
    });
}

}
