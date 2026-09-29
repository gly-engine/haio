#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * dst laid on src and kept within it, with src's alpha, and like imagemagick nothing
 * where src does not reach; the colour is mixed by dst's alpha alone.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "dst"_"rgb" "dst"_"a" + "src"_"rgb" (1 - "dst"_"a")), ("res"_"a" = "src"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dstatop -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dstatop -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::DstAtop>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Cleared,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        std::array<double, 4> out{0, 0, 0, s[3]};
        for (int c = 0; c < 3; c++) out[c] = d[c] * d[3] + s[c] * (1 - d[3]);
        return out;
    });
}

}
