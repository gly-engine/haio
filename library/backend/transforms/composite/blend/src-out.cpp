#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * src where dst is not, and like imagemagick nothing where src does not reach.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "src"_"rgb"), ("res"_"a" = "src"_"a" (1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose srcout -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose srcout -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::SrcOut>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Cleared,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        return {s[0], s[1], s[2], s[3] * (1 - d[3])};
    });
}

}
