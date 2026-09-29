#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * dst kept only where it is at least half opaque and src differs from it, which is
 * what changed between two frames; like imagemagick nothing where src does not reach.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "dst"_"rgb"), ("res"_"a" = {(0, "if " "dst"_"a" < 1/2 " or src = dst"), (1, "otherwise"):}) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose changemask -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose changemask -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::ChangeMask>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Cleared,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        // the same as imagemagick 6 with no fuzz: every channel equal, or both clear
        const bool same = s[3] == d[3] && (d[3] == 0 || (s[0] == d[0] && s[1] == d[1] && s[2] == d[2]));
        return {d[0], d[1], d[2], d[3] < 0.5 || same ? 0.0 : 1.0};
    });
}

}
