#include <haio_codec.hpp>
#include <haio/codecs/generators/xc.hpp>

#include <algorithm>

namespace Haio::Codecs {

/** a canvas of one colour; the words are what came after "xc:" */
template <>
Result<Image<Color::RGBA8888>> Generate<Brush::Xc, Color::RGBA8888>(std::string_view words, const Settings& settings) {
    HAIO_TRY(size, Canvas::sizeOf(settings));

    // imagemagick draws xc: with nothing after it in white
    HAIO_TRY(colour, Canvas::colourOf(words.empty() ? std::string_view{"white"} : words));

    auto image = Canvas::blank(size);
    for (int y = 0; y < size.height; y++) {
        for (int x = 0; x < size.width; x++) Canvas::put(image, x, y, colour);
    }
    return image;
}

}
