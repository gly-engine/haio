#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>
#include "hcl.hpp"

namespace Haio::Implementation::Transforms {

/**
 * dst's colour with src's hue, in imagemagick 6's hue, chroma and luma; Haio::Util::getHclBlend
 * has the rest.
 *
 * @note photoshop's hue is w3c's, in hue, saturation and luma with the colour clipped
 * back into range, which is also what css and opengl's advanced blends do; this is
 * imagemagick 6's hue, chroma and luma, so the colours come out different.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "hcl"("src hue, dst chroma, dst luma")), ("res"_"a" = max("src"_"a", "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose hue -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose hue -composite png:-
 * </center>
 */
BlendRes Blend<Compose::Hue, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        return Util::getHclBlend(s, d, [](Util::HclSplit from, Util::HclSplit into) {
            into.hue = from.hue;
            return into;
        });
    });
}

}
