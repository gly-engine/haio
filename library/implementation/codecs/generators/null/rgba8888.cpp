#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/implementation/codecs/generators/null.hpp>

namespace Haio::Implementation::Codecs {

/** null: is a canvas with nothing on it, and says nothing after its colon */
CodecRes<Color::RGBA8888> Generate<Brush::Null, Color::RGBA8888>::operator()(std::string_view words, const Settings& settings) const {
    if (!words.empty()) HAIO_FAIL(InvalidInput, "null: draws nothing " + Util::quoted(words));
    HAIO_TRY(size, Util::getCanvasSize(settings));
    return Util::getCanvasBlank(size);
}

}
