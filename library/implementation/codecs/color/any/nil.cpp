#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs/convert.hpp>

namespace Haio::Implementation::Codecs {

/** every colour forgets its pixels the same way, keeping only how big it was */
CodecRes<Color::NIL> Convert<Color::RGBA8888, Color::NIL>::operator()(Image<Color::RGBA8888> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}

CodecRes<Color::NIL> Convert<Color::RGB888, Color::NIL>::operator()(Image<Color::RGB888> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}

CodecRes<Color::NIL> Convert<Color::RGB565, Color::NIL>::operator()(Image<Color::RGB565> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}

CodecRes<Color::NIL> Convert<Color::RGB555, Color::NIL>::operator()(Image<Color::RGB555> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}

CodecRes<Color::NIL> Convert<Color::RGBA5551, Color::NIL>::operator()(Image<Color::RGBA5551> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}

CodecRes<Color::NIL> Convert<Color::BGR888, Color::NIL>::operator()(Image<Color::BGR888> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}

CodecRes<Color::NIL> Convert<Color::BGRA8888, Color::NIL>::operator()(Image<Color::BGRA8888> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}

CodecRes<Color::NIL> Convert<Color::GRAY8, Color::NIL>::operator()(Image<Color::GRAY8> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}

CodecRes<Color::NIL> Convert<Color::GRAYALPHA88, Color::NIL>::operator()(Image<Color::GRAYALPHA88> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}

CodecRes<Color::NIL> Convert<Color::ETC1, Color::NIL>::operator()(Image<Color::ETC1> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}

CodecRes<Color::NIL> Convert<Color::YUV420, Color::NIL>::operator()(Image<Color::YUV420> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}

CodecRes<Color::NIL> Convert<Color::PALETTE, Color::NIL>::operator()(Image<Color::PALETTE> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}

CodecRes<Color::NIL> Convert<Color::CHR_NES, Color::NIL>::operator()(Image<Color::CHR_NES> src) const {
    return Image<Color::NIL>{src.width, src.height, {}};
}
}
