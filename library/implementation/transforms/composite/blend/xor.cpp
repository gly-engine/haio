#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * each where the other is not.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = ("src"_"rgb" "src"_"a" (1 - "dst"_"a") + "dst"_"rgb" "dst"_"a" (1 - "src"_"a")) / "res"_"a"), ("res"_"a" = "src"_"a" + "dst"_"a" - 2 "src"_"a" "dst"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose xor -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose xor -composite png:-
 * </center>
 */
BlendRes Blend<Compose::Xor, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        const double ra = s[3] + d[3] - 2 * s[3] * d[3];
        std::array<double, 4> out{0, 0, 0, ra};
        for (int c = 0; c < 3; c++) out[c] = (s[c] * s[3] * (1 - d[3]) + d[c] * d[3] * (1 - s[3])) * reciprocalOf(ra);
        return out;
    });
}

}
