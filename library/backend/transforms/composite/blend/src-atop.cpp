#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * src laid on dst and kept within it, with dst's alpha; the colour is mixed by src's
 * alpha alone, as imagemagick 6 does.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "src"_"rgb" "src"_"a" + "dst"_"rgb" (1 - "src"_"a")), ("res"_"a" = "dst"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose srcatop -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose srcatop -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::SrcAtop>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        std::array<double, 4> out{0, 0, 0, d[3]};
        for (int c = 0; c < 3; c++) out[c] = s[c] * s[3] + d[c] * (1 - s[3]);
        return out;
    });
}

}
