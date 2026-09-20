#include <haio_codec.hpp>

#include <spng.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>

namespace {

std::optional<Haio::Error> checkSpng(int err, std::string_view message) {
    if (!err) return std::nullopt;
    return Haio::Error{Haio::ErrorCode::InvalidInput, std::string(message) + ": " + spng_strerror(err)};
}

}

namespace Haio::Codecs {
/**
 * @addtogroup encode
 * @{
 */
/**
 * @name Convert
 * @{
 */
template <>
Result<Blob> Encode<Format::PNG, Color::RGBA8888>(Image<Color::RGBA8888> img) {
    const auto expected = static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4;
    if (img.width <= 0 || img.height <= 0 || img.data.size() != expected) {
        HAIO_FAIL(InvalidInput, "invalid rgba8888 image for png encode");
    }

    std::unique_ptr<spng_ctx, decltype(&spng_ctx_free)> ctx(spng_ctx_new(SPNG_CTX_ENCODER), spng_ctx_free);
    if (!ctx) HAIO_FAIL(InvalidInput, "failed to create png writer");

    HAIO_CHECK(checkSpng(spng_set_option(ctx.get(), SPNG_ENCODE_TO_BUFFER, 1), "failed to configure png writer"));

    spng_ihdr ihdr{};
    ihdr.width = static_cast<uint32_t>(img.width);
    ihdr.height = static_cast<uint32_t>(img.height);
    ihdr.bit_depth = 8;
    ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
    ihdr.compression_method = 0;
    ihdr.filter_method = 0;
    ihdr.interlace_method = SPNG_INTERLACE_NONE;

    HAIO_CHECK(checkSpng(spng_set_ihdr(ctx.get(), &ihdr), "failed to set png header"));
    HAIO_CHECK(checkSpng(spng_encode_image(ctx.get(), img.data.data(), img.data.size(), SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE), "failed to encode png"));

    size_t size = 0;
    int err = 0;
    std::unique_ptr<void, decltype(&std::free)> bytes(spng_get_png_buffer(ctx.get(), &size, &err), std::free);
    HAIO_CHECK(checkSpng(err, "failed to read encoded png"));
    if (!bytes) HAIO_FAIL(InvalidInput, "failed to read encoded png");

    const auto* begin = static_cast<const uint8_t*>(bytes.get());
    std::vector<uint8_t> buffer(begin, begin + size);
    return Blob{Format::PNG, Color::RGBA8888, "image/png", {}, std::move(buffer)};
}
/** @} */
/**
 @}
 */
}
