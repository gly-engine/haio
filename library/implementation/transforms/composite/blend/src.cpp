#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * src as it is, and like imagemagick nothing where it does not reach.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "src"_"rgb"), ("res"_"a" = "src"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose src -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose src -composite png:-
 * </center>
 */
BlendRes Blend<Compose::Src, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendPixels(std::move(dst), src, x, y, Outside::Cleared,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        (void)d;
        return s;
    });
}

}
