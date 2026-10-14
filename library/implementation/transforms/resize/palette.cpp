#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/resize.hpp>

namespace Haio::Implementation::Transforms {

/**
 * point is the only filter a page of indices can take: there is no mixing two of
 * them. any other filter is refused here, and the pipeline drops the indices rather
 * than keeping a picture that no longer matches the full colour one.
 */
Result<Image<Color::PALETTE>> Transform<Operation::Resize, Color::PALETTE>::operator()(Image<Color::PALETTE> image, Size size, Transform<>::ResizeFilter filter) const {
    if (size.width <= 0 && size.height <= 0) HAIO_FAIL(InvalidInput, "resize expects a positive size");
    if (filter != Transform<>::ResizeFilter::Point) HAIO_FAIL(InvalidInput, "a palette is only ever resized by point");
    return resizeNearest(image, size);
}

}
