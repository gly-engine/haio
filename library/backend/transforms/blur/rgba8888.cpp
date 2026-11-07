#include <haio/transforms/blur.hpp>

namespace Haio::Transforms {

template <>
Result<Image<Color::RGBA8888>> Blur<Color::RGBA8888>(Image<Color::RGBA8888> image, int radius) {
    if (radius < 0) HAIO_FAIL(InvalidInput, "blur expects a radius of zero or more");
    return boxBlur(std::move(image), radius);
}

}
