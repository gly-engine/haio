#include <haio/transforms/crop.hpp>

namespace Haio::Transforms {

template <>
Result<Image<Color::RGBA8888>> Crop<Color::RGBA8888>(Image<Color::RGBA8888> image, Rect rect) {
    return cropImage(image, rect);
}

}
