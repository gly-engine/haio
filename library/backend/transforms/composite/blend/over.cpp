#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

/**
 * @startuml{math}
 * {: ("result"_"rgb" = ("src"_"rgb" "src"_"a" + "dst"_"rgb" "dst"_"a" (1 - "src"_"a")) / "result"_"a"), ("result"_"a" = "src"_"a" + "dst"_"a"(1 - "src"_"a")) :}
 * @enduml
 */
template <>
Result<Image<Color::RGBA8888>> Blend<Compose::Over>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    return blendWith(std::move(base), layer, x, y,
                     [](double sc, double sa, double dc, double da) { return sa >= 1 ? sc : sc * sa + dc * da * (1 - sa); },
                     unionOf);
}

}
