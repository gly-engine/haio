#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs/convert.hpp>

namespace Haio::Implementation::Codecs {

/** the same three bytes the other way round, and an alpha nobody stored */
Result<void> Move<Color::BGR888, Color::RGBA8888>::operator()(Bytes src, std::span<uint8_t> dst, Size size) const {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 3 || dst.size() < pixels * 4) {
        HAIO_FAIL(InvalidInput, "bgr888 to rgba8888 got a buffer that is too small");
    }

    for (size_t at = 0, to = 0; at < pixels * 3; at += 3, to += 4) {
        dst[to + 0] = src[at + 2];
        dst[to + 1] = src[at + 1];
        dst[to + 2] = src[at + 0];
        dst[to + 3] = 255;
    }
    return {};
}

CodecRes<Color::RGBA8888> Convert<Color::BGR888, Color::RGBA8888>::operator()(Image<Color::BGR888> src) const {
    return convertVia<Color::BGR888, Color::RGBA8888>(std::move(src));
}
}
