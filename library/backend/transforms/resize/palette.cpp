#include <haio/transforms/resize.hpp>

namespace Haio::Transforms {

/**
 * point is the only filter a page of indices can take: there is no mixing two of
 * them. any other filter is refused here, and the pipeline drops the indices rather
 * than keeping a picture that no longer matches the full colour one.
 */
template <>
Result<Image<Color::PALETTE>> Resize<Color::PALETTE>(Image<Color::PALETTE> image, Size size, ResizeFilter filter) {
    if (size.width <= 0 && size.height <= 0) HAIO_FAIL(InvalidInput, "resize expects a positive size");
    if (filter != ResizeFilter::Point) HAIO_FAIL(InvalidInput, "a palette is only ever resized by point");
    return resizeNearest(image, size);
}

}
