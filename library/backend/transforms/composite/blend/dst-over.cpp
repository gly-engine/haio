#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * over with the two swapped: dst laid on src.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = ("dst"_"rgb" "dst"_"a" + "src"_"rgb" "src"_"a" (1 - "dst"_"a")) / "res"_"a"), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dstover -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dstover -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::DstOver>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        const double ra = unionOf(s[3], d[3]);
        std::array<double, 4> out{0, 0, 0, ra};
        for (int c = 0; c < 3; c++) out[c] = (d[c] * d[3] + s[c] * s[3] * (1 - d[3])) * reciprocalOf(ra);
        return out;
    });
}

}
