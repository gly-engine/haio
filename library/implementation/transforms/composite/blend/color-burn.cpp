#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * the colour where both are opaque; blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = {(1, "if " "dst"_"rgb" = 1), (0, "if " "src"_"rgb" = 0), (1 - min(1, (1 - "dst"_"rgb") / "src"_"rgb"), "otherwise"):}), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose colorburn -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose colorburn -composite png:-
 * </center>
 */
BlendRes Blend<Compose::ColorBurn, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) {
        if (dc >= 1) return 1.0;
        if (sc <= 0) return 0.0;
        return 1 - std::min(1.0, (1 - dc) / sc);
    });
}

}
