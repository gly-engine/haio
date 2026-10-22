#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * an rgb picture has no black channel to copy into, so like imagemagick 6 with
 * anything that is not cmyk this leaves dst as it is.
 *
 * @todo does something only for a cmyk picture, which haio has no colour for yet.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "dst"_"rgb"), ("res"_"a" = "dst"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyblack -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyblack -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::CopyBlack>(BlendDst<> dst, BlendSrc src, int x, int y) {
    (void)src, (void)x, (void)y;
    return dst;
}

}
