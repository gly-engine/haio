#include <haio/transforms/radius.hpp>

namespace Haio::Transforms {

template <>
Result<Image<Color::RGBA8888>> Radius<Color::RGBA8888>(Image<Color::RGBA8888> image, int radius) {
    return roundImageCorners(image, radius);
}

}
