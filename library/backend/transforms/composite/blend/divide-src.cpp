#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * the colour where both are opaque; Haio::blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "dst"_"rgb" / "src"_"rgb"), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dividesrc -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dividesrc -composite png:-
 * </center>
 */
template <>
BlendRes Blend<Compose::DivideSrc>(BlendDst<> dst, BlendSrc src, int x, int y) {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) {
        // as imagemagick 6 asks it, so black over black is black and anything else over black white
        if (sc == 0) return dc == 0 ? 0.0 : 1.0;
        return dc / sc;
    });
}

}
