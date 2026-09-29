#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the sum of the two, premultiplied, wrapping past white back to black rather than
 * stopping at it; it is not divided back by the alpha, as imagemagick 6 does not.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = ("src"_"rgb" "src"_"a" + "dst"_"rgb" "dst"_"a") mod 1), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose modulusadd -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose modulusadd -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::ModulusAdd>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        const double ra = unionOf(s[3], d[3]);
        std::array<double, 4> out{0, 0, 0, ra};
        for (int c = 0; c < 3; c++) {
            const double sum = s[c] * s[3] + d[c] * d[3];
            out[c] = sum <= 1 ? sum : sum - 1;
        }
        return out;
    });
}

}
