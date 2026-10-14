#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/negate.hpp>

namespace Haio::Implementation::Transforms {

Result<Image<Color::RGBA8888>> Transform<Operation::Negate, Color::RGBA8888>::operator()(Image<Color::RGBA8888> image) const {
    return negateImage(std::move(image));
}

}
