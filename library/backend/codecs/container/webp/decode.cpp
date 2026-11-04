#include <haio_codec.hpp>
#include <haio/generated/codec.hpp>
#include <haio_convert.hpp>

#include <webp/decode.h>

#include <vector>

namespace {

Haio::Result<WebPBitstreamFeatures> featuresOf(const Haio::Blob& blob) {
    WebPBitstreamFeatures features{};

    if (WebPGetFeatures(blob.data.data(), blob.data.size(), &features) != VP8_STATUS_OK) {
        HAIO_FAIL(InvalidInput, "this is not a webp haio can read");
    }
    if (features.has_animation) {
        HAIO_FAIL(UnsupportedFormat, "haio reads a picture, and this webp is an animation");
    }
    if (features.width <= 0 || features.height <= 0) {
        HAIO_FAIL(InvalidInput, "the webp has no area");
    }
    return features;
}

}

namespace Haio::Codecs {

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Decode<Format::WEBP, Color::RGBA8888>(const Blob& blob) {
    HAIO_TRY(features, featuresOf(blob));

    const int width = features.width;
    const int height = features.height;
    HAIO_TRY(bytes, sizeOf(Color::RGBA8888, Size{width, height}));
    std::vector<uint8_t> pixels(bytes);

    if (!WebPDecodeRGBAInto(blob.data.data(), blob.data.size(), pixels.data(), pixels.size(), width * 4)) {
        HAIO_FAIL(InvalidInput, "the webp could not be decoded");
    }
    return Image<Color::RGBA8888>{width, height, std::move(pixels)};
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
/** the planes sit back to back, which is the layout Color::YUV420 is defined as */
template <>
Result<Image<Color::YUV420>> Decode<Format::WEBP, Color::YUV420>(const Blob& blob) {
    HAIO_TRY(features, featuresOf(blob));

    const int width = features.width;
    const int height = features.height;
    HAIO_TRY(bytes, sizeOf(Color::YUV420, Size{width, height}));
    std::vector<uint8_t> planes(bytes);

    const size_t luma = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t chroma = (bytes - luma) / 2;
    const int chromaStride = width / 2;

    if (!WebPDecodeYUVInto(blob.data.data(), blob.data.size(),
                           planes.data(), luma, width,
                           planes.data() + luma, chroma, chromaStride,
                           planes.data() + luma + chroma, chroma, chromaStride)) {
        HAIO_FAIL(InvalidInput, "the webp could not be decoded");
    }
    return Image<Color::YUV420>{width, height, std::move(planes)};
}
/** @} */

}
