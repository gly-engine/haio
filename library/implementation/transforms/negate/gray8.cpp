#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/negate.hpp>

namespace Haio::Implementation::Transforms {

Result<Image<Color::GRAY8>> Transform<Operation::Negate, Color::GRAY8>::operator()(Image<Color::GRAY8> image) const {
    return negateImage(std::move(image));
}

}
