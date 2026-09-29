#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup convert
 * @{
 */
/** every colour forgets its pixels the same way, keeping only how big it was */
template <>
Result<Image<Color::NIL>> Convert<Color::RGBA8888, Color::NIL>(Image<Color::RGBA8888> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}

template <>
Result<Image<Color::NIL>> Convert<Color::RGB888, Color::NIL>(Image<Color::RGB888> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}

template <>
Result<Image<Color::NIL>> Convert<Color::RGB565, Color::NIL>(Image<Color::RGB565> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}

template <>
Result<Image<Color::NIL>> Convert<Color::RGB555, Color::NIL>(Image<Color::RGB555> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}

template <>
Result<Image<Color::NIL>> Convert<Color::RGBA5551, Color::NIL>(Image<Color::RGBA5551> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}

template <>
Result<Image<Color::NIL>> Convert<Color::BGR888, Color::NIL>(Image<Color::BGR888> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}

template <>
Result<Image<Color::NIL>> Convert<Color::BGRA8888, Color::NIL>(Image<Color::BGRA8888> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}

template <>
Result<Image<Color::NIL>> Convert<Color::GRAY8, Color::NIL>(Image<Color::GRAY8> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}

template <>
Result<Image<Color::NIL>> Convert<Color::GRAYALPHA88, Color::NIL>(Image<Color::GRAYALPHA88> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}

template <>
Result<Image<Color::NIL>> Convert<Color::ETC1, Color::NIL>(Image<Color::ETC1> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}

template <>
Result<Image<Color::NIL>> Convert<Color::YUV420, Color::NIL>(Image<Color::YUV420> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}

template <>
Result<Image<Color::NIL>> Convert<Color::PALETTE, Color::NIL>(Image<Color::PALETTE> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}

template <>
Result<Image<Color::NIL>> Convert<Color::CHR_NES, Color::NIL>(Image<Color::CHR_NES> src) {
    return Image<Color::NIL>{src.width, src.height, {}};
}
/** @} */

}
