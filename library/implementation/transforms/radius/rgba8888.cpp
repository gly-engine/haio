#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/radius.hpp>

namespace Haio::Implementation::Transforms {

Result<Image<Color::RGBA8888>> Transform<Operation::Radius, Color::RGBA8888>::operator()(Image<Color::RGBA8888> image, int radius) const {
    return roundImageCorners(image, radius);
}

}
