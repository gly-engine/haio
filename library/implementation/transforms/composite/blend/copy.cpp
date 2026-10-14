#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * src as it is where it reaches, and dst where it does not.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "src"_"rgb"), ("res"_"a" = "src"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copy -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copy -composite png:-
 * </center>
 */
BlendRes Blend<Compose::Copy, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        (void)d;
        return s;
    });
}

}
