#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

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
template <>
BlendRes Blend<Compose::CopyBlue>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendPixels(std::move(dst), src, x, y, Outside::Kept,
                       [](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        auto out = d;
        out[2] = s[2];
        return out;
    });
}

}
