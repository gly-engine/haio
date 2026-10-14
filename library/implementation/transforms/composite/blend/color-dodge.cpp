#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * the colour where both are opaque; blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = {(1, "if " "dst"_"rgb" + "src"_"rgb" >= 1), ("dst"_"rgb" / (1 - "src"_"rgb"), "otherwise"):}), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose colordodge -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose colordodge -composite png:-
 * </center>
 */
BlendRes Blend<Compose::ColorDodge, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) {
        // as imagemagick 6 asks it, so white over black is white
        if (sc + dc >= 1) return 1.0;
        return dc / (1 - sc);
    });
}

}
