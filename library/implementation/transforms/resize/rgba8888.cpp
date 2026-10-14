#include <haio/generated/transforms/classes.hpp>
#include <haio/implementation/transforms/resize.hpp>

namespace Haio::Implementation::Transforms {

Result<Image<Color::RGBA8888>> Transform<Operation::Resize, Color::RGBA8888>::operator()(Image<Color::RGBA8888> image, Size size, Transform<>::ResizeFilter filter) const {
    if (size.width <= 0 && size.height <= 0) HAIO_FAIL(InvalidInput, "resize expects a positive size");
    switch (filter) {
        case Transform<>::ResizeFilter::Point: return resizeNearest(image, size);
    }
    HAIO_FAIL(InvalidInput, "this resize filter does not run on rgba8888");
}

}
