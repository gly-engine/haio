#include <haio/transforms/negate.hpp>

namespace Haio::Transforms {

template <>
Result<Image<Color::GRAY8>> Negate<Color::GRAY8>(Image<Color::GRAY8> image) {
    return negateImage(std::move(image));
}

}
