#include <haio/transforms/composite.hpp>

namespace Haio::Transforms {

template <>
Result<Image<Color::RGBA8888>> Blend<Compose::Plus>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y) {
    return blendWith(std::move(base), layer, x, y,
                     [](double sc, double sa, double dc, double da) { return sc * sa + dc * da; },
                     [](double sa, double da) { return sa + da; });
}

}
