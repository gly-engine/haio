#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * nothing at all, where src reaches and where it does not.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = 0), ("res"_"a" = 0) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose clear -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose clear -composite png:-
 * </center>
 */
BlendRes Blend<Compose::Clear, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendPixels(std::move(dst), src, x, y, Outside::Cleared,
                       [](const std::array<double, 4>&, const std::array<double, 4>&) { return std::array<double, 4>{}; });
}

}
