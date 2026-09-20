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
        /** @todo yuv420 is 1.5 bytes per pixel and needs even dimensions; no codec
            produces it yet, so the rule is not written down anywhere but here */
        case Color::YUV420: return std::unexpected(Error{ErrorCode::UnsupportedFormat, "yuv420 has no size rule yet"});
        default: break;
    }
    return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unknown colour"});
}

}
