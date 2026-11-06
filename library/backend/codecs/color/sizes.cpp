#include <haio/internal/codecs/yuv.hpp>
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

    // every etc and eac is whole four by four blocks, which is one rule for all of them
    if (Util::GPU::blockBytes(color) != 0) return Util::GPU::blockedSize(color, size);

    switch (color) {
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
         * three planes: a luma and two chroma at half of it in both directions. an
         * odd side has no whole two by two block to end on, so the planes are grown
         * to the next even number -- see haio/internal/codecs/yuv.hpp, which is also
         * the layout libjpeg-turbo hands its planes over in.
         */
        case Color::YUV420: return Codecs::Yuv420::planesOf(size).bytes;
        case Color::NIL: return 0;
        default: break;
    }
    return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unknown colour"});
}

}
