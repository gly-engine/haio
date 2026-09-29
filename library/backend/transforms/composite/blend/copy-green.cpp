#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * src's green channel in dst, the rest of dst as it was.
 *
 * @startuml{math}
 * {: ("res"_"g" = "src"_"g"), ("res"_"rb" = "dst"_"rb"), ("res"_"a" = "dst"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copygreen -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copygreen -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::CopyGreen>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        auto out = d;
        out[1] = s[1];
        return out;
    });
}

}
