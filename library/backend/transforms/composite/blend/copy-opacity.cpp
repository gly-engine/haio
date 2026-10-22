#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * src's alpha on dst's colour, or src's intensity when src has no alpha of its own,
 * which is how a grey picture becomes a mask; like imagemagick nothing where src does
 * not reach.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "dst"_"rgb"), ("res"_"a" = {("src"_"a", "if src has an alpha"), ("src"_"y", "otherwise"):}) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyopacity -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyopacity -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::CopyOpacity>(BlendDst<> dst, BlendSrc src, int x, int y) {
    // a picture that is opaque everywhere is one that had no alpha to copy
    bool mask = true;
    for (size_t at = 3; at < src.data.size() && mask; at += 4) mask = src.data[at] == 255;
    return blendPixels(std::move(dst), src, x, y, Outside::Cleared,
                       [mask](const std::array<double, 4>& s, const std::array<double, 4>& d) -> std::array<double, 4> {
        return {d[0], d[1], d[2], mask ? intensityOf(s) : s[3]};
    });
}

}
