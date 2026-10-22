#include <haio/transforms/resize.hpp>

namespace Haio::Transforms {

template <>
Result<Image<Color::RGBA8888>> Resize<Color::RGBA8888>(Image<Color::RGBA8888> image, Size size, ResizeFilter filter) {
    if (size.width <= 0 && size.height <= 0) HAIO_FAIL(InvalidInput, "resize expects a positive size");
    switch (filter) {
        case ResizeFilter::Point: return resizeNearest(image, size);
    }
    HAIO_FAIL(InvalidInput, "this resize filter does not run on rgba8888");
}

}
