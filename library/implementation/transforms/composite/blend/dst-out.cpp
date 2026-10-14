#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * dst where src is not.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "dst"_"rgb"), ("res"_"a" = "dst"_"a" (1 - "src"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dstout -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dstout -composite png:-
 * </center>
 */
BlendRes Blend<Compose::DstOut, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        return {d[0], d[1], d[2], d[3] * (1 - s[3])};
    });
}

}
