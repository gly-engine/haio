#include <haio/implementation/language/stages/pix_fmt.hpp>
#include <haio/implementation/language/token.hpp>
#include <haio/implementation/registry.hpp>

namespace Haio::Implementation::LanguageStage {

template <>
Built build<&pixFmt>(std::string_view value, const Given&) {
    const auto color = colorNamed(value);
    if (!color) return std::unexpected(Refusal{"unrecognized pixel format " + Util::quoted(value), std::string(value)});
    return LanguageToken::PixFmt(*color);
}

}
