#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * src less dst, premultiplied, wrapping past black back to white. imagemagick 6 has
 * the wrap the wrong way round and ends up with black or white; this is the wrap it means.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = ("src"_"rgb" "src"_"a" - "dst"_"rgb" "dst"_"a") mod 1), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose modulussubtract -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose modulussubtract -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::ModulusSubtract>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        const double ra = unionOf(s[3], d[3]);
        std::array<double, 4> out{0, 0, 0, ra};
        for (int c = 0; c < 3; c++) {
            const double difference = s[c] * s[3] - d[c] * d[3];
            out[c] = difference < 0 ? difference + 1 : difference;
        }
        return out;
    });
}

}
