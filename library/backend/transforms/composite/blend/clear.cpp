#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

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
template <>
BlendRes Blend<Compose::Clear>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Cleared,
                       [](const std::array<double, 4>&, const std::array<double, 4>&) { return std::array<double, 4>{}; });
}

}
