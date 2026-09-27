#include <haio/transforms/crop.hpp>

namespace Haio::Transforms {

/**
 * the same rectangle of the same picture: indices are addressable, so there is
 * nothing here that full colour has and they do not, and the palette rides along.
 */
template <>
Result<Image<Color::PALETTE>> Crop<Color::PALETTE>(Image<Color::PALETTE> image, Rect rect) {
    return cropImage(image, rect);
}

}
