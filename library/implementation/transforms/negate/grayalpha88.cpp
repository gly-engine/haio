#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/negate.hpp>

namespace Haio::Implementation::Transforms {

Result<Image<Color::GRAYALPHA88>> Transform<Operation::Negate, Color::GRAYALPHA88>::operator()(Image<Color::GRAYALPHA88> image) const {
    return negateImage(std::move(image));
}

}
