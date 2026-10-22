#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * yellow is the blue channel of an rgb picture, so this is copyblue, as imagemagick 6 has it
 * for anything that is not cmyk.
 *
 * @todo different from copyblue only for a cmyk picture, which haio has no colour for yet.
 *
 * @startuml{math}
 * {: ("res"_"b" = "src"_"b"), ("res"_"rg" = "dst"_"rg"), ("res"_"a" = "dst"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyyellow -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyyellow -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::CopyYellow>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        auto out = d;
        out[2] = s[2];
        return out;
    });
}

}
