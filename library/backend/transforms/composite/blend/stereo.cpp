#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * an anaglyph: src's red with dst's green and blue, dst being the left eye. the alpha
 * is imagemagick 6's, which makes dst half as transparent again.
 *
 * @startuml{math}
 * {: ("res"_"r" = "src"_"r"), ("res"_"gb" = "dst"_"gb"), ("res"_"a" = 1 - 3/2 (1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose stereo -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose stereo -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::Stereo>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        return {s[0], d[1], d[2], std::max(0.0, 1 - 1.5 * (1 - d[3]))};
    });
}

}
