#include <haio/internal/codecs/yuv.hpp>
#include <haio/transforms/blur.hpp>

namespace Haio::Transforms {

/**
 * each plane on its own, so a jpeg is blurred without going through rgba and back.
 * the chroma planes are half the size of the picture, so they take half the radius,
 * rounded up so that a blur of one still reaches the colour.
 *
 * luma is blurred over the real pixels only, and the spare column and row of an odd
 * side are copied again afterwards, since they have to stay a copy of their neighbour.
 */
template <>
Result<Image<Color::YUV420>> Blur<Color::YUV420>(Image<Color::YUV420> image, int radius) {
    if (radius < 0) HAIO_FAIL(InvalidInput, "blur expects a radius of zero or more");
    if (image.width <= 0 || image.height <= 0) HAIO_FAIL(InvalidInput, "blur got an empty yuv420 picture");

    const Size size{image.width, image.height};
    const auto planes = Codecs::Yuv420::planesOf(size);
    if (image.data.size() < planes.bytes) HAIO_FAIL(InvalidInput, "yuv420 picture is smaller than its planes");
    if (radius == 0) return image;

    auto* luma = image.data.data();
    boxBlur<1>(luma, size.width, size.height, static_cast<size_t>(planes.lumaWidth), radius);
    Codecs::Yuv420::repeatEdges(image.data, size, planes);

    const int half = (radius + 1) / 2;
    for (size_t plane = 0; plane < 2; plane++) {
        boxBlur<1>(luma + planes.luma + plane * planes.chroma, planes.chromaWidth, planes.chromaHeight,
                   static_cast<size_t>(planes.chromaWidth), half);
    }
    return image;
}

}
