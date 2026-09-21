#include <haio_convert.hpp>
#include <haio_util.hpp>

namespace Haio {

Result<size_t> sizeOf(Color color, Size size) {
    if (size.width <= 0 || size.height <= 0) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "image needs positive dimensions"});
    }

    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);

    // the addressable colours are just their stride, which strideOf already knows
    if (const auto stride = strideOf(color); stride != 0) return pixels * stride;

    switch (color) {
        case Color::ETC1: return Util::GPU::etc1Size(size);

        /**
         * sixteen bytes for every eight by eight tile, which works out to two bits a
         * pixel. the sides have to be whole tiles: a chr file has no way to say that
         * the last row is short, so an odd size would be read past the end.
         */
        case Color::CHR_NES: {
            if (size.width % 8 != 0 || size.height % 8 != 0) {
                return std::unexpected(Error{ErrorCode::InvalidInput,
                                             "nes chr is made of 8x8 tiles, so both sides must be multiples of 8"});
            }
            return (static_cast<size_t>(size.width) / 8) * (static_cast<size_t>(size.height) / 8) * 16;
        }

        /**
         * three planes: a full size luma and two half size chroma. the halves are
         * why the dimensions have to be even, and refusing an odd one here is better
         * than rounding it and handing back a picture a line short.
         */
        case Color::YUV420: {
            if (size.width % 2 != 0 || size.height % 2 != 0) {
                return std::unexpected(Error{ErrorCode::InvalidInput,
                                             "yuv420 needs even width and height"});
            }
            const auto chroma = (static_cast<size_t>(size.width) / 2) * (static_cast<size_t>(size.height) / 2);
            return pixels + chroma * 2;
        }
        default: break;
    }
    return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unknown colour"});
}

}
