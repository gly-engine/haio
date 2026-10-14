#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

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
BlendRes Blend<Compose::Dst, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    (void)src, (void)x, (void)y;
    return dst;
}

}
