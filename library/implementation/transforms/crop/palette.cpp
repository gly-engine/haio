#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/crop.hpp>

namespace Haio::Implementation::Transforms {

/**
 * the same rectangle of the same picture: indices are addressable, so there is
 * nothing here that full colour has and they do not, and the palette rides along.
 */
Result<Image<Color::PALETTE>> Transform<Operation::Crop, Color::PALETTE>::operator()(Image<Color::PALETTE> image, Rect rect) const {
    return cropImage(image, rect);
}

}
