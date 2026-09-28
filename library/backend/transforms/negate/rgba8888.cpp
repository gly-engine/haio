#include <haio/transforms/negate.hpp>

namespace Haio::Transforms {

template <>
Result<Image<Color::RGBA8888>> Negate<Color::RGBA8888>(Image<Color::RGBA8888> image) {
    return negateImage(std::move(image));
}

}
