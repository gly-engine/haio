#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs/convert.hpp>

namespace Haio::Implementation::Codecs {

/** red and blue trade places and the alpha stays where it was */
Result<void> Move<Color::BGRA8888, Color::RGBA8888>::operator()(Bytes src, std::span<uint8_t> dst, Size size) const {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 4 || dst.size() < pixels * 4) {
        HAIO_FAIL(InvalidInput, "bgra8888 to rgba8888 got a buffer that is too small");
    }

    for (size_t at = 0; at < pixels * 4; at += 4) {
        dst[at + 0] = src[at + 2];
        dst[at + 1] = src[at + 1];
        dst[at + 2] = src[at + 0];
        dst[at + 3] = src[at + 3];
    }
    return {};
}

CodecRes<Color::RGBA8888> Convert<Color::BGRA8888, Color::RGBA8888>::operator()(Image<Color::BGRA8888> src) const {
    return convertVia<Color::BGRA8888, Color::RGBA8888>(std::move(src));
}
}
