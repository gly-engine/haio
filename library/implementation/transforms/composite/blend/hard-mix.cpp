#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * black or white, by whether the two add up to white; premultiplied and not blended
 * by the alpha, as imagemagick 6 does it.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = {(0, "if " "src"_"rgb" "src"_"a" + "dst"_"rgb" "dst"_"a" < 1), (1, "otherwise"):}), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose hardmix -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose hardmix -composite png:-
 * </center>
 */
BlendRes Blend<Compose::HardMix, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        const double ra = unionOf(s[3], d[3]);
        std::array<double, 4> out{0, 0, 0, ra};
        for (int c = 0; c < 3; c++) out[c] = s[c] * s[3] + d[c] * d[3] < 1 ? 0 : reciprocalOf(ra);
        return out;
    });
}

}
