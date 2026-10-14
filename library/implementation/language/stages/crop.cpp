#include <haio/implementation/language/stages/crop.hpp>
#include <haio/implementation/transforms/crop.hpp>
#include <haio/implementation/language/token.hpp>
#include <haio/util/string.hpp>

namespace Haio::Implementation::LanguageStage {

/** both ways of writing a rectangle, since both are already out there */
template <>
Built build<&crop>(std::string_view value, const Given&) {
    Rect rect;
    if (!Util::tryGetCropGeometry(value, rect) && !Util::tryGetRect(value, rect)) {
        return std::unexpected(Refusal{"invalid argument for option `-crop': " + std::string(value), std::string(value)});
    }
    return LanguageToken::Crop(rect);
}

}
