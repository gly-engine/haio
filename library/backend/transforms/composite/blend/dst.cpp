#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * dst as it is: src changes nothing.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "dst"_"rgb"), ("res"_"a" = "dst"_"a") :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dst -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dst -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::Dst>(BlendDst<> dst, BlendSrc src, int x, int y) {
    (void)src, (void)x, (void)y;
    return dst;
}

}
