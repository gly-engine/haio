#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

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
BlendRes Blend<Compose::CopyMagenta, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        auto out = d;
        out[1] = s[1];
        return out;
    });
}

}
