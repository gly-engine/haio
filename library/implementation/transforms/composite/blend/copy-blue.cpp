#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * src's blue channel in dst, the rest of dst as it was.
 *
 * @startuml{math}
 * {: ("res"_"b" = "src"_"b"), ("res"_"rg" = "dst"_"rg"), ("res"_"a" = "dst"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyblue -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyblue -composite png:-
 * </center>
 */
BlendRes Blend<Compose::CopyBlue, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        auto out = d;
        out[2] = s[2];
        return out;
    });
}

}
