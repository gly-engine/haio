#include <haio_codec.hpp>
#include <haio/codecs/generators/null.hpp>

namespace Haio::Codecs {

/** null: is a canvas with nothing on it, and says nothing after its colon */
template <>
Result<Image<Color::RGBA8888>> Generate<Brush::Null, Color::RGBA8888>(std::string_view words, const Settings& settings) {
    if (!words.empty()) HAIO_FAIL(InvalidInput, "null: draws nothing " + Stages::quoted(words));
    HAIO_TRY(size, Canvas::sizeOf(settings));
    return Canvas::blank(size);
}

}
