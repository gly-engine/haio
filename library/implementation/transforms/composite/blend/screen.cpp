#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/composite.hpp>

namespace Haio::Implementation::Transforms {

/**
 * the colour where both are opaque; blendSeparable has the rest.
 *
 * @startuml{math}
 * "res"_"rgba" = 1 - (1 - "dst"_"rgba")(1 - "src"_"rgba")
 * @enduml
 *
 * <center>
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose screen -composite png:-
 * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose screen -composite png:-
 * </center>
 */
BlendRes Blend<Compose::Screen, Color::RGBA8888, Color::RGBA8888>::operator()(BlendDst<> dst, BlendSrc src, int x, int y) const {
    return blendSeparable(std::move(dst), src, x, y, [](double sc, double dc) { return sc + dc - sc * dc; });
}

}
