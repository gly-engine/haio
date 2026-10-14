#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * the colour where both are opaque; blendSeparable has the rest.
 *
 * @startuml{math}
 * {: ("res"_"rgb" = "dst"_"rgb" + 2 "src"_"rgb" - 1), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose linearlight -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose linearlight -composite png:-
 * </center>
 */
BlendRes Blend<Compose::LinearLight, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) { return dc + 2 * sc - 1; });
}

}
