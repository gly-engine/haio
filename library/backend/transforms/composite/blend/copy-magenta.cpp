#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * magenta is the green channel of an rgb picture, so this is copygreen, as imagemagick 6 has it
 * for anything that is not cmyk.
 *
 * @todo different from copygreen only for a cmyk picture, which haio has no colour for yet.
 *
 * @startuml{math}
 * {: ("res"_"g" = "src"_"g"), ("res"_"rb" = "dst"_"rb"), ("res"_"a" = "dst"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copymagenta -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copymagenta -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::CopyMagenta>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        auto out = d;
        out[1] = s[1];
        return out;
    });
}

}
