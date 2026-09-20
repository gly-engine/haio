#include <haio_codec.hpp>
#include <haio_codecs.hpp>
#include <haio_convert.hpp>

#include <string>

namespace Haio::Codecs {

/**
 * @addtogroup encode
 * @{
 */
/**
 * takes rgba and drops the alpha on the way out, so the caller does not have to
 * convert first. the header and the pixels share one buffer: Move writes straight
 * into the tail instead of building a second image to copy from.
 */
template <>
Result<Blob> Encode<Format::PPM, Color::RGBA8888>(Image<Color::RGBA8888> img) {
    const Size size{img.width, img.height};
    HAIO_TRY(incoming, sizeOf(Color::RGBA8888, size));
    if (img.data.size() != incoming) HAIO_FAIL(InvalidInput, "invalid rgba8888 image for ppm encode");

    HAIO_TRY(pixels, sizeOf(Color::RGB888, size));
    const auto header = "P6\n" + std::to_string(img.width) + " " + std::to_string(img.height) + "\n255\n";

    std::vector<uint8_t> out(header.size() + pixels);
    std::copy(header.begin(), header.end(), out.begin());

    if (auto moved = Move<Color::RGBA8888, Color::RGB888>(img.data, std::span{out}.subspan(header.size()), size); !moved) {
        return std::unexpected(moved.error());
    }
    return Blob{Format::PPM, Color::RGB888, "image/x-portable-pixmap", {}, std::move(out)};
}
/** @} */

}
