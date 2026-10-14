#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/crop.hpp>

namespace Haio::Implementation::Transforms {

Result<Image<Color::RGBA8888>> Transform<Operation::Crop, Color::RGBA8888>::operator()(Image<Color::RGBA8888> image, Rect rect) const {
    return cropImage(image, rect);
}

}
