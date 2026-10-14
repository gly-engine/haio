#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs/convert.hpp>

namespace Haio::Implementation::Codecs {

Result<void> Move<Color::GRAYALPHA88, Color::RGBA8888>::operator()(Bytes src, std::span<uint8_t> dst, Size size) const {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 2 || dst.size() < pixels * 4) {
        HAIO_FAIL(InvalidInput, "grayalpha88 to rgba8888 got a buffer that is too small");
    }
    for (size_t at = 0, to = 0; at < pixels * 2; at += 2, to += 4) {
        dst[to + 0] = src[at];
        dst[to + 1] = src[at];
        dst[to + 2] = src[at];
        dst[to + 3] = src[at + 1];
    }
    return {};
}

CodecRes<Color::RGBA8888> Convert<Color::GRAYALPHA88, Color::RGBA8888>::operator()(Image<Color::GRAYALPHA88> src) const {
    return convertVia<Color::GRAYALPHA88, Color::RGBA8888>(std::move(src));
}
}
