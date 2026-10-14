#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * @class Blend<Compose::Bumpmap, Color::RGBA8888, Color::RGBA8888>
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "src"_"y" "dst"_"rgb"), ("res"_"a" = 1 - "src"_"y" (1 - "src"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose bumpmap -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose bumpmap -composite png:-
 * </center>
 */
BlendRes Blend<Compose::Bumpmap, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        if (s[3] == 0) return d;
        const double level = intensityOf(s);
        return {level * d[0], level * d[1], level * d[2], 1 - level * (1 - s[3])};
    });
}

}
