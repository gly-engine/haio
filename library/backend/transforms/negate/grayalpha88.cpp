#include <haio/transforms/negate.hpp>

namespace Haio::Transforms {

template <>
Result<Image<Color::GRAYALPHA88>> Negate<Color::GRAYALPHA88>(Image<Color::GRAYALPHA88> image) {
    return negateImage(std::move(image));
}

}
