#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs/convert.hpp>

namespace Haio::Implementation::Codecs {

/** one sample becomes three, opaque. grey is grey in every channel */
Result<void> Move<Color::GRAY8, Color::RGBA8888>::operator()(Bytes src, std::span<uint8_t> dst, Size size) const {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels || dst.size() < pixels * 4) {
        HAIO_FAIL(InvalidInput, "gray8 to rgba8888 got a buffer that is too small");
    }
    for (size_t at = 0, to = 0; at < pixels; at++, to += 4) {
        const auto level = src[at];
        dst[to + 0] = level;
        dst[to + 1] = level;
        dst[to + 2] = level;
        dst[to + 3] = 0xFF;
    }
    return {};
}

CodecRes<Color::RGBA8888> Convert<Color::GRAY8, Color::RGBA8888>::operator()(Image<Color::GRAY8> src) const {
    return convertVia<Color::GRAY8, Color::RGBA8888>(std::move(src));
}
}
