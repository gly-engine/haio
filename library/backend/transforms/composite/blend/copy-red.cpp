#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * src's red channel in dst, the rest of dst as it was.
 *
 * @startuml{math}
 * {: ("res"_"r" = "src"_"r"), ("res"_"gb" = "dst"_"gb"), ("res"_"a" = "dst"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyred -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyred -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::CopyRed>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        auto out = d;
        out[0] = s[0];
        return out;
    });
}

}
