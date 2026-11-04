#include <haio_codec.hpp>
#include <haio/generated/codec.hpp>
#include <haio_convert.hpp>
#include <haio/codecs/webp.hpp>

#include <webp/encode.h>

#include <vector>

namespace {

struct Memory {
    WebPMemoryWriter raw;
    Memory() { WebPMemoryWriterInit(&raw); }
    ~Memory() { WebPMemoryWriterClear(&raw); }
};

/** planes pointed at an Image are the Image's, and WebPPictureFree never took them */
struct Picture {
    WebPPicture raw{};
    bool ok = WebPPictureInit(&raw);
    ~Picture() { if (ok) WebPPictureFree(&raw); }
};

Haio::Result<WebPConfig> configFor(const Haio::Settings& settings) {
    using namespace Haio;
    using namespace Haio::Codecs;

    HAIO_TRY(quality, settingInt(settings, webpQuality));
    HAIO_TRY(method, settingInt(settings, webpMethod));

    auto compression = WebpCompression::Lossy;
    if (const auto* named = settingNamed(settings, webpCompression.name())) {
        compression = *Stages::enumNamed<WebpCompression>(named->value);
    }

    WebPConfig config{};
    if (!WebPConfigInit(&config)) HAIO_FAIL(Internal, "cannot start the webp encoder");

    config.quality = static_cast<float>(quality);
    config.method = method;
    config.lossless = compression == WebpCompression::Lossless;

    if (!WebPValidateConfig(&config)) HAIO_FAIL(Internal, "the webp encoder refused these settings");
    return config;
}

Haio::Result<Haio::Blob> written(const WebPConfig& config, Picture& picture, Haio::Color color) {
    Memory memory;
    picture.raw.writer = WebPMemoryWrite;
    picture.raw.custom_ptr = &memory.raw;

    if (!WebPEncode(&config, &picture.raw)) {
        HAIO_FAIL(Internal, "the webp could not be encoded");
    }
    return Haio::Blob{Haio::Format::WEBP, color, "image/webp", {},
                      std::vector<uint8_t>(memory.raw.mem, memory.raw.mem + memory.raw.size)};
}

}

namespace Haio::Codecs {

/**
 * @addtogroup encode
 * @{
 */
/** argb either way: vp8l is argb, and vp8 converts it itself rather than twice here */
template <>
Result<Blob> Encode<Format::WEBP, Color::RGBA8888>(Image<Color::RGBA8888> img, const Settings& settings) {
    HAIO_TRY(expected, sizeOf(Color::RGBA8888, Size{img.width, img.height}));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "invalid rgba8888 image for webp encode");

    HAIO_TRY(config, configFor(settings));

    Picture picture;
    if (!picture.ok) HAIO_FAIL(Internal, "cannot start the webp encoder");

    picture.raw.width = img.width;
    picture.raw.height = img.height;
    picture.raw.use_argb = 1;

    if (!WebPPictureImportRGBA(&picture.raw, img.data.data(), img.width * 4)) {
        HAIO_FAIL(Internal, "cannot hold a picture that size");
    }
    return written(config, picture, Color::RGBA8888);
}
/** @} */

/**
 * @addtogroup encode
 * @{
 */
/**
 * "-pix_fmt yuv420", which is a jpeg's road to a webp without becoming rgb on the
 * way: the encoder reads the image's own planes. it is not the default because an
 * rgba picture sent this way would lose its alpha quietly.
 */
template <>
Result<Blob> Encode<Format::WEBP, Color::YUV420>(Image<Color::YUV420> img, const Settings& settings) {
    HAIO_TRY(expected, sizeOf(Color::YUV420, Size{img.width, img.height}));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "invalid yuv420 image for webp encode");

    HAIO_TRY(config, configFor(settings));

    Picture picture;
    if (!picture.ok) HAIO_FAIL(Internal, "cannot start the webp encoder");

    const size_t luma = static_cast<size_t>(img.width) * static_cast<size_t>(img.height);
    const size_t chroma = (expected - luma) / 2;

    picture.raw.width = img.width;
    picture.raw.height = img.height;
    picture.raw.use_argb = 0;
    picture.raw.colorspace = WEBP_YUV420;
    picture.raw.y = img.data.data();
    picture.raw.u = img.data.data() + luma;
    picture.raw.v = img.data.data() + luma + chroma;
    picture.raw.y_stride = img.width;
    picture.raw.uv_stride = img.width / 2;

    return written(config, picture, Color::YUV420);
}
/** @} */

}
