#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * cyan is the red channel of an rgb picture, so this is copyred, as imagemagick 6 has it
 * for anything that is not cmyk.
 *
 * @todo different from copyred only for a cmyk picture, which haio has no colour for yet.
 *
 * @startuml{math}
 * {: ("res"_"r" = "src"_"r"), ("res"_"gb" = "dst"_"gb"), ("res"_"a" = "dst"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copycyan -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copycyan -composite png:-
 * </center>
 */
BlendRes Blend<Compose::CopyCyan, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        auto out = d;
        out[0] = s[0];
        return out;
    });
}

}
