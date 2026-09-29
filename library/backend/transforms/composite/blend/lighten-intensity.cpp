#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * whichever of the two pixels is lighter, by its intensity weighed by its alpha,
 * taken whole.
 *
 * @note photoshop's lighter colour picks the same way but by the sum of the
 * channels, where imagemagick 6 weighs them by rec.709.
 *
 * @startuml{math}
 * {: ("res" = {("src", "if " "src"_"a" "src"_"y" > "dst"_"a" "dst"_"y"), ("dst", "otherwise"):}) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose lightenintensity -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose lightenintensity -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::LightenIntensity>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        return s[3] * intensityOf(s) > d[3] * intensityOf(d) ? s : d;
    });
}

}
