#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/implementation/codecs/generators/xc.hpp>

#include <algorithm>

namespace Haio::Implementation::Codecs {

/** a canvas of one colour; the words are what came after "xc:" */
CodecRes<Color::RGBA8888> Generate<Brush::Xc, Color::RGBA8888>::operator()(std::string_view words, const Settings& settings) const {
    HAIO_TRY(size, Util::getCanvasSize(settings));

    // imagemagick draws xc: with nothing after it in white
    HAIO_TRY(colour, Util::getCanvasColour(words.empty() ? std::string_view{"white"} : words));

    auto image = Util::getCanvasBlank(size);
    for (int y = 0; y < size.height; y++) {
        for (int x = 0; x < size.width; x++) Util::setCanvasPixel(image, x, y, colour);
    }
    return image;
}

}
